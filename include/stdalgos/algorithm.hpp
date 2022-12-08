// TODO: License and copyright
//
// Requirements:
// - must be able to use scheduler from environment (whether
//   get_completion_scheduler or through some connect-mechanism)
// - must be able to customize at the top-level (i.e. full algorithm) based on
//   scheduler

// TODO: Should the callable be sent by the sender or provided immediately?
// Precedent: then takes f immediately, the value by sender.
//
// TODO: Where and how do execution_policy constraints get applied?
//
// TODO: Are other hints/properties required besides seq/par/par_unseq?
//
// TODO: Some schedulers can provide only maximum seq, no par. Other schedulers
// can provide only par/par_unseq, no seq (is this true? can use various
// fallbacks...). How would one recreate (i.e. get exactly the same behaviour)
// std::for_each(std::execution::par, b, e, f)
// using the asynchronous overloads?
//
// transfer_just(system_context().get_scheduler(), b, e) |
//   stdalgos::for_each(std::execution::par, f)?
//
// just(b, e) | stdalgos::for_each(std::execution::par, f)?
//
// transfer_just(with_policy(system_context().get_scheduler(), std::execution::par), b, e) |
//   stdalgos::for_each(f)?
//
// Should for_each above be sequential? Probably yes. The policy on the
// scheduler may not be visible to the writer at the for_each call. Or should
// the par set the default and it can be overridden at the for_each callsite?
//
// TODO: Should seq/par/par_unseq have a relation to forward progress guarantees
// of a scheduler?
//
// NOTE: Comparison to HPX's mechanism:
// HPX bundles all "properties" into the execution policy for parallel
// algorithms, including whether or not a particular invocation is asynchronous
// or not:
// - hpx::execution::par: use parallel policy with default scheduler
// - par.on(executor/scheduler): use parallel policy on a particular executor
//   (the executor advertises if it supports a given policy)
// - par(task).on(executor): the invocation will return a future
// - par(task).on(executor).with(chunk_size): apply a given chunk size to the
//   invocation
// - par(task).on(with_priority/stacksize(executor)).with(chunk_size): apply a
//   given priority/stacksize to the executor
//
// Differences to what's below:
// - synchronous/asynchronous execution is handled by the same(-looking)
//   overloads in HPX. This should be separate overloads (potentially in
//   different namespaces).
// - the executor/scheduler is provided per-call in HPX. This should be provided
//   with the same mechanism as all other P2300 algorithms
//   (get_completion_scheduler/connect?).
//
// NOTE: Comparison to Kokkos' mechanism:
// Kokkos also bundles all "properties" into an execution policy, including
// where something runs. Kokkos does not have fundamental support for
// asynchronous execution, though in practice it does happen with some execution
// spaces (which requires fencing eventually; think async_scope with
// start_detached followed by sync_wait on async_scope::empty). Kokkos also
// combines the iteration pattern into the execution policy.
// - Kokkos::RangePolicy(): default execution space
// - Kokkos::RangePolicy<Kokkos::Cuda>(): a specific execution space
// - Kokkos::RangePolicy<Kokkos::Cuda>().set_chunk_size(chunk_size): use a given
//   chunk size
// - require(Kokkos::RangePolicy<Kokkos::Cuda>(), hint): use a given hint

#pragma once

#include <execution>
#include <stdexec/execution.hpp>
#include <stdexec/functional.hpp>

namespace stdalgos {
namespace detail {
template <typename> struct is_execution_property : std::false_type {};
template <typename P>
concept execution_property = is_execution_property<P>::value;

// TODO: Let scheduler be provided from the environment/sender. Pack all other
// properties, including the execution policy, into something else? This _must_
// be extensible so that one can add scheduler-specific properties. Requiring it
// be extensible is one argument for just bundling it with the scheduler instead
// and providing some sort of env from it.
//
// An argument against bundling it with the scheduler is that execution policy,
// chunk sizes etc. are call-specific (may depend on data, callable, iteration
// range, etc.). However, the scheduler and/or tag_invoke customizations with
// the scheduler must still at some point be allowed to access the data at some
// point.
//
// Compromise: schedulers are fundamentally what hold properties, but they can
// be overridden on a per-call basis.
template <typename... Properties>
requires
    // clang-format off
    (execution_property<Properties> &&...)
    // clang-format on
    struct execution_properties {
  std::tuple<std::decay_t<Properties>...> properties;
};

template <typename... Properties> auto make_execution_properties(Properties &&...properties) {
  return execution_properties<std::decay_t<Properties>...>(
      {std::forward<Properties>(properties)...});
}

template <typename ExecutionPolicy>
concept execution_policy =
    std::same_as<std::decay_t<ExecutionPolicy>, std::execution::sequenced_policy> ||
    std::same_as<std::decay_t<ExecutionPolicy>, std::execution::unsequenced_policy> ||
    std::same_as<std::decay_t<ExecutionPolicy>, std::execution::parallel_policy> ||
    std::same_as<std::decay_t<ExecutionPolicy>, std::execution::parallel_unsequenced_policy>;

template <execution_policy ExecutionPolicy>
struct is_execution_property<ExecutionPolicy> : std::true_type {};

// TODO: This is an attempt at a lightweight property mechanism for
// schedulers. This can probably take many forms. The important thing is that
// they can be required on schedulers/parallel algorithm call sites.
//
// Open questions:
// - open or closed set of properties? (most likely open)
// - if open, should missing properties be errors or not?
struct with_execution_property_t {
  // All properties that have a customization for the given scheduler are
  // applied.
  template <stdexec::scheduler Scheduler, execution_property ExecutionProperty>
  requires
      // clang-format off
      (stdexec::tag_invocable<with_execution_property_t, Scheduler const &, ExecutionProperty>)
      // clang-format on
      auto
      operator()(Scheduler const &sched, ExecutionProperty &&exec_property) const {
    return stdexec::tag_invoke(with_execution_property_t{}, sched,
                               std::forward<ExecutionProperty>(exec_property));
  }

  // Custom properties that can't be applied to the given scheduler are ignored.
  // TODO: Is this wise? See prefer/require from the various properties
  // proposals.
  template <stdexec::scheduler Scheduler, execution_property ExecutionProperty>
  requires
      // clang-format off
      (!stdexec::tag_invocable<with_execution_property_t, Scheduler const &, ExecutionProperty>)
      // clang-format on
      decltype(auto)
      operator()(Scheduler &&sched, ExecutionProperty &&exec_property) const {
    return std::forward<Scheduler>(sched);
  }

  // TODO: Is an execution policy a required property of schedulers that are
  // passed to parallel algorithms?

  template <stdexec::scheduler Scheduler, execution_property ExecutionProperty0,
            execution_property... Rest>
  requires
      // clang-format off
      (!stdexec::tag_invocable<with_execution_property_t, Scheduler const &, ExecutionProperty0, Rest...>)
      // clang-format on
      auto
      operator()(stdexec::scheduler auto const &sched, ExecutionProperty0 &&exec_property0,
                 Rest &&...rest) const {
    return with_execution_property_t{}(
        with_execution_property_t{}(sched, std::forward<ExecutionProperty0>(exec_property0)),
        std::forward<Rest>(rest)...);
  };
};

// TODO: forwarding/moving/const/ref is not consistent.
template <stdexec::scheduler Scheduler, typename... Properties>
auto with_execution_properties(Scheduler const &sched,
                               execution_properties<Properties...> exec_properties) {
  return std::apply(
      [&](auto &&...p) {
        return with_execution_properties(sched, std::forward<decltype(p)>(p)...);
      },
      std::move(exec_properties.properties));
}

struct for_each_t {
  // TODO: What is the basis set? What are the fundamental overloads? This is
  // currently a bit too free-form...
  //
  // synchronous overloads:
  // - existing: nothing
  // - existing: execution policy
  //
  // - new: execution properties on their own? (apply to system scheduler?)
  // - new: scheduler? (basis)
  //
  // asynchronous overloads:
  // - new: nothing (equivalent to passing get_completion_scheduler if one exists, otherwise system scheduler?)
  // - new: execution policy? (local override for the current algorithm only? special case of one single execution property, gets applied to get_completion_scheduler if exists, otherwise system scheduler?)
  // - new: execution properties? (local overrides for the current algorithm only? generalization of the above, gets applied to get_completion_scheduler if exists, otherwise system scheduler?)
  // - new: scheduler? (basis)
  //
  // The following lists a possible customization hierarchy. algo() mark
  // forwarding calls to the algorithm itself. [...] mark customizations. This
  // doesn't quite make full sense yet.
  // - Does the order make sense?
  // - Are the system_scheduler fallbacks necessary?
  // - Are so many levels necessary? Can some levels be left out to keep things simple?
  // - Can the policy and properties overloads be collapsed (they're essentially the same)?
  // - Do the property/policy-less overloads default to seq or use some default from scheduler?
  //
  // 1. algo(f, ...) -> for_each(seq, f, ...) [not customizable]
  // 2. algo(policy, f, ...) -> algo(properties(policy), f, ...) [not customizable]
  // 3. algo(properties, f, ...) -> algo(with(system_scheduler, properties), f, ...) [not customizable]
  // 4. [this is P2500] algo(scheduler, f, ...) -> [scheduler customization] or
  //                                               sync_wait(algo(scheduler, f, just(...)))
  // 5. algo(sender, f) -> [completion_scheduler(sender) customization] or
  //                       [sender customization (with properties(seq)?)] or
  //                       algo(completion_scheduler(sender), sender, f) or
  //                       algo(system_scheduler, f, sender)
  // 6. algo(sender, policy, f) -> algo(sender, properties(policy), f) [not customizable]
  // 7. algo(sender, properties, f) -> [with(completion_scheduler(sender), properties) customization] or
  //                                   [sender customization] or
  //                                   algo(with(completion_scheduler(sender), properties), sender, f) or
  //                                   algo(with(system_scheduler, properties), sender, f)
  // 8. algo(scheduler, sender, f) -> [scheduler customization] or
  //                                  [sender customization] or
  //                                  default implementation
  //
  // The above exposes three customization points, which are used in this order
  // (though not all are applicable in all situations):
  // 1. tag_invoke(algo_t, scheduler, f, ...)
  // 2. tag_invoke(algo_t, scheduler, sender, additional_properties, f)
  // 3. tag_invoke(algo_t, sender, additional_properties, f)

  // Synchronous overloads

  // Overload with scheduler and tag_invoke customization: This exists to allow
  // optimizing the synchronous case. In most cases it should not be necessary
  // to customize this.
  template <stdexec::scheduler Scheduler, typename It, typename F>
  requires
      // clang-format off
      (stdexec::tag_invocable<for_each_t, Scheduler, It, It, F>)
      // clang-format on
      void
      operator()(Scheduler &&sched, It b, It e, F &&f) const {
    // This allows for specializing/optimizing the synchronous case directly
    stdexec::tag_invoke(for_each_t{}, std::forward<Scheduler>(sched), b, e, std::forward<F>(f));
  }

  // Default synchronous implementation with a scheduler but without a
  // tag_invoke customization: This falls back to sync_waiting one of the
  // asynchronous overloads.
  template <stdexec::scheduler Scheduler, typename It, typename F>
  requires
      // clang-format off
      (!stdexec::tag_invocable<for_each_t, Scheduler, It, It, F>)
      // clang-format on
      void
      operator()(Scheduler &&sched, It b, It e, F &&f) const {
    stdexec::this_thread::sync_wait(for_each_t{}(
        stdexec::transfer_just(std::forward<Scheduler>(sched), b, e), std::forward<F>(f)));
  }

  // This should be identical to the existing for_each(exec_policy, ...)
  // overload in std. Included only for completeness. This is not meant to be
  // customized. It should fall back to the very default implementation.
  template <execution_policy ExecutionPolicy, typename It, typename F>
  void operator()(ExecutionPolicy &&exec_policy, It b, It e, F &&f) const {
    // Fall back to synchronizing the asynchronous overload if no synchronous
    // customization is available.
    stdexec::this_thread::sync_wait(
        for_each_t{}(make_execution_properties(std::forward<ExecutionPolicy>(exec_policy)),
                     stdexec::just(b, e), std::forward<F>(f)));
  }

  // Asynchronous overloads

  // Overload when there is a tag_invoke overload and a completion scheduler
  // from the sender.
  template <stdexec::sender Sender, typename F>
  requires
      // clang-format off
      (stdexec::__tag_invocable_with_completion_scheduler<for_each_t, stdexec::set_value_t, Sender, F>)
      // clang-format on
      stdexec::sender auto
      operator()(Sender &&sender, F &&f) const {
    return stdexec::tag_invoke(stdexec::get_completion_scheduler<stdexec::set_value_t>(sender),
                               std::forward<Sender>(sender), std::forward<F>(f));
  }

  // Sender customization
  template <stdexec::sender Sender, typename F>
  requires
      // clang-format off
      (!stdexec::__tag_invocable_with_completion_scheduler<for_each_t, stdexec::set_value_t, Sender, F>) &&
      (stdexec::tag_invocable<for_each_t, Sender, F>)
      // clang-format on
      stdexec::sender auto
      operator()(Sender &&sender, F &&f) const {
    return stdexec::tag_invoke(std::forward<Sender>(sender), std::forward<F>(f));
  }

  // Forward to default implementation with default execution properties
  template <stdexec::sender Sender, typename F>
  requires
      // clang-format off
      (!stdexec::__tag_invocable_with_completion_scheduler<for_each_t, stdexec::set_value_t, Sender, F>) &&
      (!stdexec::tag_invocable<for_each_t, Sender, F>)
      // clang-format on
      stdexec::sender auto
      operator()(Sender &&sender, F &&f) const {
    return for_each_t{}(execution_properties<>{}, std::forward<Sender>(sender), std::forward<F>(f));
  }

  // Default implementation
  template <stdexec::sender Sender, typename F, typename... Properties>
  requires
      // clang-format off
      (!stdexec::__tag_invocable_with_completion_scheduler<for_each_t, stdexec::set_value_t, Sender, F>) &&
      (!stdexec::tag_invocable<for_each_t, Sender, F>)
      // clang-format on
      stdexec::sender auto
      operator()(execution_properties<Properties...> const &exec_properties, Sender &&sender,
                 F &&f) const {
    // TODO: Should a (completion) scheduler be required?
    if constexpr (stdexec::__has_completion_scheduler<Sender, stdexec::set_value_t>) {
      stdexec::scheduler auto sched =
          stdexec::get_completion_scheduler<stdexec::set_value_t>(sender);
      return stdexec::let_value(
          std::forward<Sender>(sender),
          [f = std::forward<F>(f), sched = std::move(sched), exec_properties](auto &b, auto &e) {
            // TODO: Is it allowed for this to be executed on an arbitrary
            // execution context?
            auto n = std::distance(b, e);

            // TODO: This could use something a bit nicer.
            stdexec::scheduler auto sched_with_properties =
                with_execution_properties(sched, exec_properties);

            return stdexec::schedule(std::move(sched)) |
                   stdexec::bulk(n, [b = std::move(b), e = std::move(e), f = std::move(f)](auto i) {
                     f(b[i]);
                   });
          });
    } else {
      // If there's no completion scheduler there's nothing to apply the
      // execution policy or properties to.
      // TODO: Should this fall back to the system_context scheduler? Possibly
      // yes, especially if the execution policy is par.
      return stdexec::let_value(
          std::forward<Sender>(sender), [f = std::forward<F>(f)](auto &b, auto &e) {
            auto n = std::distance(b, e);
            return stdexec::just() | stdexec::bulk(n, [b = std::move(b), e = std::move(e),
                                                       f = std::move(f)](auto i) { f(b[i]); });
          });
    }
  }
};

// The following don't necessarily have to be specified, but it must be possible
// to use the same principles for them in the future.
// TODO: Ranges overloads?
// TODO: std::linalg overloads?
// TODO: Delegation from one algorithm to another? Guarantee or allow? What
// about compile-times?
} // namespace detail

using detail::execution_properties;
using detail::make_execution_properties;

using for_each_t = detail::for_each_t;
inline constexpr for_each_t for_each{};
} // namespace stdalgos

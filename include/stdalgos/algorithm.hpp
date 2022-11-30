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
template <typename ExecutionPolicy> struct execution_properties {
  ExecutionPolicy policy;
  // etc.
};

template <typename ExecutionPolicy>
concept execution_policy =
    std::same_as<std::decay_t<ExecutionPolicy>, std::execution::sequenced_policy> ||
    std::same_as<std::decay_t<ExecutionPolicy>, std::execution::unsequenced_policy> ||
    std::same_as<std::decay_t<ExecutionPolicy>, std::execution::parallel_policy> ||
    std::same_as<std::decay_t<ExecutionPolicy>, std::execution::parallel_unsequenced_policy>;

// TODO: This is an attempt at a lightweight property mechanism for
// schedulers.
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
      (!execution_policy<ExecutionProperty>) &&
      (!stdexec::tag_invocable<with_execution_property_t, Scheduler const &, ExecutionProperty>)
      // clang-format on
      decltype(auto)
      operator()(Scheduler &&sched, ExecutionProperty &&exec_property) const {
    return std::forward<Scheduler>(sched);
  }

  // All schedulers must support setting the execution policy for parallel
  // algorithms. Not being able to do so is an error.
  template <stdexec::scheduler Scheduler, execution_property ExecutionProperty>
  requires
      // clang-format off
      (execution_policy<ExecutionProperty>) &&
      (!stdexec::tag_invocable<with_execution_property_t, Scheduler const &, ExecutionProperty>)
      // clang-format on
      auto
      operator()(stdexec::scheduler auto const &sched,
                 ExecutionProperty &&exec_property) const = delete;
};

struct for_each_t {
  // Overloads have the following priority:
  // 1. synchronous tag_invoke customization
  // 2. synchronus default implementation which sync_waits asynchronous
  //    overload
  // 3. asynchronous tag_invoke customization with completion scheduler
  // 4. asynchronous tag_invoke overload on sender
  // 4. asynchronous default implementation based on bulk

  // Synchronous overload
  template <stdexec::scheduler Scheduler, typename It, typename F>
  requires
      // clang-format off
      (stdexec::tag_invocable<for_each_t, Scheduler, It, It, F>)
      // clang-format on
      stdexec::sender auto
      operator()(Scheduler &&sched, It b, It e, F &&f) const {
    // This allows for specializing/optimizing the synchronous case directly
    stdexec::tag_invoke(*this, std::forward<Scheduler>(sched), b, e, std::forward<F>(f));
  }

  template <stdexec::scheduler Scheduler, typename It, typename F>
  requires
      // clang-format off
      (!stdexec::tag_invocable<for_each_t, Scheduler, It, It, F>)
      // clang-format on
      stdexec::sender auto
      operator()(Scheduler &&sched, It b, It e, F &&f) const {
    // Fall back to synchronizing the asynchronous overload if no synchronous
    // customization is available.
    stdexec::this_thread::sync_wait(for_each_t{}(
        stdexec::transfer_just(std::forward<Scheduler>(sched), b, e), std::forward<F>(f)));
  }

  // Asynchronous overloads

  // Scheduler customization
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

  // Forward to default implementation with seq policy
  template <stdexec::sender Sender, typename F>
  requires
      // clang-format off
      (!stdexec::__tag_invocable_with_completion_scheduler<for_each_t, stdexec::set_value_t, Sender, F>) &&
      (!stdexec::tag_invocable<for_each_t, Sender, F>)
      // clang-format on
      stdexec::sender auto
      operator()(Sender &&sender, F &&f) const {
    return for_each_t{}(std::execution::seq, std::forward<Sender>(sender), std::forward<F>(f));
  }

  // Default implementation
  template <typename ExecutionPolicy, stdexec::sender Sender, typename F>
  requires
      // clang-format off
      (!stdexec::__tag_invocable_with_completion_scheduler<for_each_t, stdexec::set_value_t, Sender, F>) &&
      (!stdexec::tag_invocable<for_each_t, Sender, F>) &&
      (execution_policy<ExecutionPolicy>)
      // clang-format on
      stdexec::sender auto
      operator()(ExecutionPolicy &&exec_policy, Sender &&sender, F &&f) const {
    // TODO: Should a (completion) scheduler be required?
    if constexpr (stdexec::__has_completion_scheduler<Sender, stdexec::set_value_t>) {
      stdexec::scheduler auto sched =
          stdexec::get_completion_scheduler<stdexec::set_value_t>(sender);
      return stdexec::let_value(
          std::forward<Sender>(sender),
          [f = std::forward<F>(f), sched = std::move(sched)](auto &b, auto &e) {
            // TODO: Is it allowed for this to be executed on an arbitrary
            // execution context?
            auto n = std::distance(b, e);
            // TODO: Apply execution policy to scheduler.
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

  // The following don't necessarily have to be specified, but it must be
  // possible to use the same principles for them in the future.
  // TODO: Ranges overloads?
  // TODO: std::linalg overloads?
};
} // namespace detail

using for_each_t = detail::for_each_t;
inline constexpr for_each_t for_each{};
} // namespace stdalgos

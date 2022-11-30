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
// transfer_just(system_context().get_scheduler(), b, e) | stdalgos::for_each(f)
//
// TODO: Should seq/par/par_unseq have a relation to forward progress guarantees
// of a scheduler?

#pragma once

#include <stdexec/execution.hpp>
#include <stdexec/functional.hpp>

namespace stdalgos {
namespace detail {
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
    stdexec::tag_invoke(*this, std::forward<Scheduler>(sched), b, e,
                        std::forward<F>(f));
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
        stdexec::transfer_just(std::forward<Scheduler>(sched), b, e),
        std::forward<F>(f)));
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
    return stdexec::tag_invoke(
        stdexec::get_completion_scheduler<stdexec::set_value_t>(sender),
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
    return stdexec::tag_invoke(std::forward<Sender>(sender),
                               std::forward<F>(f));
  }
  // Default implementation
  template <stdexec::sender Sender, typename F>
  requires
      // clang-format off
      (!stdexec::__tag_invocable_with_completion_scheduler<for_each_t, stdexec::set_value_t, Sender, F>) &&
      (!stdexec::tag_invocable<for_each_t, Sender, F>)
      // clang-format on
      stdexec::sender auto
      operator()(Sender &&sender, F &&f) const {
    return stdexec::let_value(
        std::forward<Sender>(sender),
        [f = std::forward<F>(f)](auto &b, auto &e) {
          auto n = std::distance(b, e);
          return stdexec::just() |
                 stdexec::bulk(n, [b = std::move(b), e = std::move(e),
                                   f = std::move(f)](auto i) { f(b[i]); });
        });
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

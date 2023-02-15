// std-async-algorithms
//
// Copyright (c) 2021-2023, ETH Zurich
// All rights reserved.
//
// Please, refer to the LICENSE file in the root directory.
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include <stdalgos/execution_properties.hpp>
#include <stdexec/execution.hpp>
#include <stdexec/functional.hpp>

namespace stdalgos {
namespace merge_detail {

struct merge_t {
  // Synchronous overloads

  template <typename InputIt1, typename InputIt2, typename OutputIt>
  OutputIt operator()(InputIt1 b1, InputIt1 e1, InputIt2 b2, InputIt2 e2,
          OutputIt d) const {
    // Fall back to synchronizing the asynchronous overload if no synchronous
    // customization is available.
    return merge_t{}(execution_properties{}, b1, e1, b2, e2, d);
  }

  // This should be identical to the existing merge(exec_policy, ...)
  // overload in std. Included only for completeness. This is not meant to be
  // customized. It should fall back to the very default implementation.
  template <typename InputIt1, typename InputIt2, typename OutputIt, typename... Properties>
  OutputIt operator()(
      execution_properties<Properties...> const &exec_properties, InputIt1 b1,
      InputIt1 e1, InputIt2 b2, InputIt2 e2, OutputIt d) const {
    // Fall back to synchronizing the asynchronous overload if no synchronous
    // customization is available.
    // The sync_wait sender needs unwrapping since it returns an optional of tuple. And this
    // overload returns an the output iterator.
    return std::get<0>(stdexec::this_thread::sync_wait(
        merge_t{}(stdexec::just(b1, e1, b2, e2, d), exec_properties)).value());
  }

  // TODO: add the overload that requires tag_invocable merge

  // Default synchronous implementation with a scheduler but without a
  // tag_invoke customization: This falls back to sync_waiting one of the
  // asynchronous overloads.
  template <stdexec::scheduler Scheduler, typename InputIt1, typename InputIt2,
        typename OutputIt>
  requires
      // clang-format off
      (!stdexec::tag_invocable<merge_t, Scheduler, InputIt1, InputIt1, InputIt2, InputIt2, OutputIt>)
      // clang-format on
      OutputIt
      operator()(Scheduler &&sched, InputIt1 b1, InputIt2 e1, InputIt2 b2, InputIt1 e2, OutputIt d) const {
         // The sync_wait sender needs unwrapping since it returns an optional of tuple. And this
         // overload returns an the output iterator.
         return std::get<0>(stdexec::this_thread::sync_wait(merge_t{}(
            stdexec::transfer_just(std::forward<Scheduler>(sched),
                b1, e1, b2, e2, d))).value());
  }

  // Asynchronous overloads

  // Use empty execution properties by default, forward to other overloads
  template <stdexec::sender Sender>
  stdexec::sender auto operator()(Sender &&sender) const {
    return merge_t{}(std::forward<Sender>(sender), execution_properties<>{});
  }

  // TODO: Add the overloads with tag_invocable and not tag_invocable with completion scheduler

  // Default implementation
  template <stdexec::sender Sender, typename... Properties>
  requires
      // clang-format off
      (!stdexec::__tag_invocable_with_completion_scheduler<merge_t, stdexec::set_value_t, Sender>) &&
      (!stdexec::tag_invocable<merge_t, Sender, execution_properties<Properties...> const&>)
      // clang-format on
      stdexec::sender auto
      operator()(Sender &&sender, execution_properties<Properties...> const
              &exec_properties) const {
    // TODO: Should a (completion) scheduler be required?
    if constexpr (stdexec::__has_completion_scheduler<Sender, stdexec::set_value_t>) {
      stdexec::scheduler auto sched =
          stdexec::get_completion_scheduler<stdexec::set_value_t>(stdexec::get_env(sender));
      return stdexec::let_value(
          std::forward<Sender>(sender), [sched = std::move(sched), exec_properties]
            (auto &b1, auto &e1, auto&b2, auto &e2, auto &d)
            {
                // TODO: This could use something a bit nicer.
                stdexec::scheduler auto sched_with_properties =
                    with_execution_properties(sched, exec_properties);

                return stdexec::schedule(std::move(sched)) |
                    stdexec::then([b1 = std::move(b1), e1 = std::move(e1),
                        b2 = std::move(b2), e2 = std::move(e2), d = std::move(d)]() mutable
                    {
                        while(b1 != e1)
                        {
                            if (*b2 < *b1)
                            {
                                d = *b2; // equivalent: *d++ = *b2;
                                ++b2;
                            }
                            else {
                                d = *b1; // equivalent: *d++ = *b1;
                                ++b1;
                            }
                            if(b2 == e2)
                                std::copy(b1, e1, d);
                        }
                        if(b1 == e1)
                            std::copy(b2, e2, d);
                        return std::move(d);
                    }
                );
          });
    } else {
      // If there's no completion scheduler there's nothing to apply the
      // execution policy or properties to.
      // TODO: Should this fall back to the system_context scheduler? Possibly
      // yes, especially if the execution policy is par.
      return stdexec::let_value(
          std::forward<Sender>(sender), [](auto &b1, auto &e1, auto &b2, auto& e2, auto& d) {
            return stdexec::just() | stdexec::then([b1 = std::move(b1), e1 = std::move(e1), b2 = std::move(b2), e2 = std::move(e2), d = std::move(d)]() mutable
                {
                    while(b1 != e1)
                    {
                        if (*b2 < *b1)
                        {
                            d = *b2; // equivalent: *d++ = *b2;
                            ++b2;
                        }
                        else {
                            d = *b1; // equivalent: *d++ = *b1;
                            ++b1;
                        }
                        if(b2 == e2)
                            std::copy(b1, e1, d);
                    }
                    if(b1 == e1)
                        std::copy(b2, e2, d);
                    return std::move(d);
                }
            );
        });
    }
    }

    template <typename... Properties>
    stdexec::__binder_back<merge_t, execution_properties<Properties...>>
    operator()(execution_properties<Properties...> const &exec_properties) const {
      return {{}, {}, {exec_properties}};
    }


    stdexec::__binder_back<merge_t> operator()() const {
      return {{}, {}};
    }

};
} // namespace merge_detail

using merge_detail::merge_t;
inline constexpr merge_t merge{};
} // namespace stdalgos

// std-async-algorithms
//
// Copyright (c) 2021-2023, ETH Zurich
// All rights reserved.
//
// Please, refer to the LICENSE file in the root directory.
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include <stdexec/execution.hpp>
#include <stdexec/functional.hpp>

namespace stdalgos {
    namespace merge_detail {

        template <typename InputIt1, typename InputIt2, typename OutputIt>
        OutputIt merge_impl(InputIt1 b1, InputIt1 e1, InputIt2 b2, InputIt2 e2, OutputIt d)
        {
            while (b1 != e1)
            {
                if (*b2 < *b1)
                {
                    d = *b2;    // equivalent: *d++ = *b2;
                    ++b2;
                }
                else
                {
                    d = *b1;    // equivalent: *d++ = *b1;
                    ++b1;
                }
                if (b2 == e2)
                    std::copy(b1, e1, d);
            }
            if (b1 == e1)
                std::copy(b2, e2, d);
            return d;
        }

        struct merge_t
        {
            // Synchronous overloads

            // This should be identical to the existing synchronous merge
            // overload in std. Included only for completeness. This is not
            // meant to be customized. It should fall back to the very default
            // implementation.
            template <typename InputIt1, typename InputIt2, typename OutputIt>
            OutputIt
            operator()(InputIt1 b1, InputIt1 e1, InputIt2 b2, InputIt2 e2, OutputIt d) const
            {
                // Fall back to synchronizing the asynchronous overload if no synchronous
                // customization is available.
                // The sync_wait sender needs unwrapping since it returns an optional of tuple. And this
                // overload returns an the output iterator.
                return std::get<0>(
                    stdexec::sync_wait(merge_t{}(stdexec::just(b1, e1, b2, e2, d))).value());
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
                operator()(Scheduler&& sched, InputIt1 b1, InputIt2 e1, InputIt2 b2, InputIt1 e2,
                    OutputIt d) const
            {
                // The sync_wait sender needs unwrapping since it returns an optional of tuple. And this
                // overload returns an the output iterator.
                return std::get<0>(
                    stdexec::sync_wait(merge_t{}(stdexec::transfer_just(
                                           std::forward<Scheduler>(sched), b1, e1, b2, e2, d)))
                        .value());
            }

            // Asynchronous overloads

            // TODO: Add the overloads with tag_invocable and not tag_invocable with completion scheduler

            // Default implementation
            template <stdexec::sender Sender>
            requires
                // clang-format off
                (!stdexec::__tag_invocable_with_completion_scheduler<merge_t, stdexec::set_value_t, Sender>) &&
                (!stdexec::tag_invocable<merge_t, Sender>)
                // clang-format on
                stdexec::sender auto
                operator()(Sender&& sender) const
            {
                // TODO: Should a (completion) scheduler be required?
                return std::forward<Sender>(sender) | stdexec::then([](auto&&... ts) {
                    return merge_impl(std::forward<decltype(ts)>(ts)...);
                });
            }

            stdexec::__binder_back<merge_t> operator()() const
            {
                return {{}, {}};
            }
        };
    }    // namespace merge_detail

    using merge_detail::merge_t;
    inline constexpr merge_t merge{};
}    // namespace stdalgos

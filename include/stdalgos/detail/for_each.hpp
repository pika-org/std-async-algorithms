// std-async-algorithms
//
// Copyright (c) 2021-2022, ETH Zurich
// All rights reserved.
//
// Please, refer to the LICENSE file in the root directory.
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include <execution>
#include <stdexec/execution.hpp>
#include <stdexec/functional.hpp>

namespace stdalgos {
    namespace for_each_detail {
        struct for_each_t
        {
            // Synchronous overloads

            // This should be identical to the existing synchronous
            // for_each(...)  overload in std. Included only for completeness.
            // This is not meant to be customized. It should fall back to the
            // very default implementation.
            template <typename It, typename F>
            void operator()(It b, It e, F&& f) const
            {
                // Fall back to synchronizing the asynchronous overload if no synchronous
                // customization is available.
                stdexec::sync_wait(for_each_t{}(stdexec::just(b, e), std::forward<F>(f)));
            }

            // Overload with scheduler and tag_invoke customization: This exists to allow
            // optimizing the synchronous case. In most cases it should not be necessary
            // to customize this.
            template <stdexec::scheduler Scheduler, typename It, typename F>
            requires
                // clang-format off
                (stdexec::tag_invocable<for_each_t, Scheduler, It, It, F>)
                // clang-format on
                void
                operator()(Scheduler&& sched, It b, It e, F&& f) const
            {
                // This allows for specializing/optimizing the synchronous case directly
                stdexec::tag_invoke(
                    for_each_t{}, std::forward<Scheduler>(sched), b, e, std::forward<F>(f));
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
                operator()(Scheduler&& sched, It b, It e, F&& f) const
            {
                // TODO: This could also use (std)exec::on
                stdexec::sync_wait(
                    for_each_t{}(stdexec::transfer_just(std::forward<Scheduler>(sched), b, e),
                        std::forward<F>(f)));
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
                operator()(Sender&& sender, F&& f) const
            {
                return stdexec::tag_invoke(stdexec::get_completion_scheduler<stdexec::set_value_t>(
                                               stdexec::get_env(sender)),
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
                operator()(Sender&& sender, F&& f) const
            {
                return stdexec::tag_invoke(std::forward<Sender>(sender), std::forward<F>(f));
            }

            // Default implementation
            template <stdexec::sender Sender, typename F>
            requires
                // clang-format off
                (!stdexec::__tag_invocable_with_completion_scheduler<for_each_t, stdexec::set_value_t, Sender, F>) &&
                (!stdexec::tag_invocable<for_each_t, Sender, F>)
                // clang-format on
                stdexec::sender auto
                operator()(Sender&& sender, F&& f) const
            {
                // TODO: Should a (completion) scheduler be required?
                if constexpr (stdexec::__has_completion_scheduler<Sender, stdexec::set_value_t>)
                {
                    stdexec::scheduler auto sched =
                        stdexec::get_completion_scheduler<stdexec::set_value_t>(
                            stdexec::get_env(sender));
                    return stdexec::let_value(std::forward<Sender>(sender),
                        [f = std::forward<F>(f), sched = std::move(sched)](auto& b, auto& e) {
                            // TODO: Is it allowed for this to be executed on an arbitrary
                            // execution context?
                            auto n = std::distance(b, e);

                            return stdexec::schedule(std::move(sched)) |
                                stdexec::bulk(n,
                                    [b = std::move(b), e = std::move(e), f = std::move(f)](
                                        auto i) { f(b[i]); });
                        });
                }
                else
                {
                    // TODO: Should this fall back to the system_context scheduler? Possibly
                    // yes, especially if the execution policy is par.
                    return stdexec::let_value(
                        std::forward<Sender>(sender), [f = std::forward<F>(f)](auto& b, auto& e) {
                            auto n = std::distance(b, e);
                            return stdexec::just() |
                                stdexec::bulk(n,
                                    [b = std::move(b), e = std::move(e), f = std::move(f)](
                                        auto i) { f(b[i]); });
                        });
                }
            }

            template <typename F>
            stdexec::__binder_back<for_each_t, F> operator()(F&& f) const
            {
                return {{}, {}, {std::forward<F>(f)}};
            }
        };
    }    // namespace for_each_detail

    using for_each_detail::for_each_t;
    inline constexpr for_each_t for_each{};
}    // namespace stdalgos

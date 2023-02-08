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

      // TODO: implement the case if completion is available
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

};
} // namespace merge_detail

using merge_detail::merge_t;
inline constexpr merge_t merge{};
} // namespace stdalgos

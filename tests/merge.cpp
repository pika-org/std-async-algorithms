// std-async-algorithms
//
// Copyright (c) 2021-2023, ETH Zurich
// All rights reserved.
//
// Please, refer to the LICENSE file in the root directory.
// SPDX-License-Identifier: BSD-3-Clause

#include <cassert>
#include <exec/on.hpp>
#include <exec/static_thread_pool.hpp>
#include <execution>
#include <iostream>
#include <iterator>
#include <stdalgos/algorithm.hpp>
#include <stdexec/execution.hpp>

#ifdef STD_ASYNC_ALGOS_HAVE_PIKA
#include <pika/execution.hpp>
#include <pika/init.hpp>
#endif

int main(int argc, char* argv[])
{
    std::vector<int> v1{1, 2, 3};
    std::vector<int> v2{1, 2, 4, 5};
    std::vector<int> dst;
    std::vector<int> dst_check;

    {
        exec::static_thread_pool pool{2};
        stdexec::scheduler auto sched = pool.get_scheduler();

        // Compute the reference result to compare results
        std::merge(v1.begin(), v1.end(), v2.begin(), v2.end(), std::back_inserter(dst_check));

        stdalgos::merge(v1.begin(), v1.end(), v2.begin(), v2.end(), std::back_inserter(dst));
        assert(std::ranges::equal(dst, dst_check));
        dst.clear();

        stdalgos::merge(sched, v1.begin(), v1.end(), v2.begin(), v2.end(), std::back_inserter(dst));
        assert(std::ranges::equal(dst, dst_check));
        dst.clear();

        {
            auto s =
                stdexec::just(v1.begin(), v1.end(), v2.begin(), v2.end(), std::back_inserter(dst)) |
                stdalgos::merge();
            stdexec::sync_wait(std::move(s));
            assert(std::ranges::equal(dst, dst_check));
            dst.clear();
        }

        {
            auto s =
                stdexec::just(v1.begin(), v1.end(), v2.begin(), v2.end(), std::back_inserter(dst)) |
                exec::on(sched, stdalgos::merge());
            stdexec::sync_wait(std::move(s));
            assert(std::ranges::equal(dst, dst_check));
            dst.clear();
        }
    }
}

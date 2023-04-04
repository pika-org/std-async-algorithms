// std-async-algorithms
//
// Copyright (c) 2021-2022, ETH Zurich
// All rights reserved.
//
// Please, refer to the LICENSE file in the root directory.
// SPDX-License-Identifier: BSD-3-Clause

#include <exec/on.hpp>
#include <exec/static_thread_pool.hpp>
#include <execution>
#include <iostream>
#include <stdalgos/algorithm.hpp>
#include <stdexec/execution.hpp>

#ifdef STD_ASYNC_ALGOS_HAVE_PIKA
#include <pika/execution.hpp>
#include <pika/init.hpp>
#endif

int main(int argc, char* argv[])
{
    std::vector<int> v{1, 2, 3};

    {
        exec::static_thread_pool pool{2};
        stdexec::scheduler auto sched = pool.get_scheduler();

        stdalgos::for_each(v.begin(), v.end(), [](int x) { std::cerr << "x = " << x << '\n'; });

        stdalgos::for_each(
            sched, v.begin(), v.end(), [](int x) { std::cerr << "x = " << x << '\n'; });

        {
            auto s = stdexec::just(v.begin(), v.end()) |
                stdalgos::for_each([](int x) { std::cerr << "x = " << x << '\n'; });
            stdexec::sync_wait(std::move(s));
        }

        {
            auto s = stdexec::just(v.begin(), v.end()) |
                exec::on(
                    sched, stdalgos::for_each([](int x) { std::cerr << "x = " << x << '\n'; }));
            stdexec::sync_wait(std::move(s));
        }
    }

#ifdef STD_ASYNC_ALGOS_HAVE_PIKA
    {
        pika::start(nullptr, argc, argv);

        {
            pika::execution::experimental::thread_pool_scheduler sched{};

            stdalgos::for_each(
                sched, v.begin(), v.end(), [](int x) { std::cerr << "x = " << x << '\n'; });

            {
                auto s = stdexec::just(v.begin(), v.end()) |
                    exec::on(
                        sched, stdalgos::for_each([](int x) { std::cerr << "x = " << x << '\n'; }));
                stdexec::sync_wait(std::move(s));
            }
        }

        pika::finalize();
        pika::stop();
    }
#endif
}

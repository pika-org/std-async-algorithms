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
#include <stdalgos/execution_properties.hpp>
#include <stdexec/execution.hpp>

#ifdef STD_ASYNC_ALGOS_HAVE_PIKA
#include <pika/execution.hpp>
#include <pika/init.hpp>
#endif

bool constexpr print = true;

template <typename T>
void check_identical(std::vector<T> &v1, std::vector<T> &v2)
{
    assert(v1.size() == v2.size());
    auto i2 = v2.begin();
    for(auto &i1 : v1)
    {
        assert(i1 == *i2);
        if constexpr(print)
            std::cerr << i1 << " ";
        ++i2;
    }
    if constexpr(print)
        std::cerr << std::endl;
}

int main(int argc, char *argv[]) {
  std::vector<int> v1{1, 2, 3};
  std::vector<int> v2{1, 2, 4, 5};
  std::vector<int> dst;
  std::vector<int> dst_check;

  {
    exec::static_thread_pool pool{2};
    stdexec::scheduler auto sched = pool.get_scheduler();

    // TODO: It should be possible to pass a plain execution_policy without
    // explicitly wrapping it in execution_properties.

    // Compute the reference result to compare results
    std::merge(v1.begin(), v1.end(), v2.begin(), v2.end(), std::back_inserter(dst_check));

    stdalgos::merge(v1.begin(), v1.end(), v2.begin(), v2.end(), std::back_inserter(dst));
    check_identical(dst, dst_check);
  }

}
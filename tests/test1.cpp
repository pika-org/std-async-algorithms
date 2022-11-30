// TODO: License and copyright

#include <exec/static_thread_pool.hpp>
#include <iostream>
#include <stdalgos/algorithm.hpp>
#include <stdexec/execution.hpp>

int main() {
  exec::static_thread_pool pool{2};
  stdexec::scheduler auto sched = pool.get_scheduler();

  std::vector<int> v{1, 2, 3};
  stdexec::this_thread::sync_wait(
      stdalgos::for_each(stdexec::just(v.begin(), v.end()),
                         [](int x) { std::cerr << "x = " << x << '\n'; }));
}

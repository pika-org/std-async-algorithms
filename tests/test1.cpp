// TODO: License and copyright

#include <exec/static_thread_pool.hpp>
#include <execution>
#include <iostream>
#include <stdalgos/algorithm.hpp>
#include <stdexec/execution.hpp>

int main() {
  exec::static_thread_pool pool{2};
  stdexec::scheduler auto sched = pool.get_scheduler();

  auto p = std::execution::par;

  std::vector<int> v{1, 2, 3};
  // TODO: It should be possible to pass a plain execution_policy without
  // explicitly wrapping it in execution_properties.
  stdexec::this_thread::sync_wait(stdalgos::for_each(
      stdalgos::make_execution_properties(std::execution::par), stdexec::just(v.begin(), v.end()),
      [](int x) { std::cerr << "x = " << x << '\n'; }));

  stdexec::this_thread::sync_wait(stdalgos::for_each(
      stdexec::just(v.begin(), v.end()), [](int x) { std::cerr << "x = " << x << '\n'; }));
}

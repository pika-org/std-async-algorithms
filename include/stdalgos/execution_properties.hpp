// std-async-algorithms
//
// Copyright (c) 2021-2022, ETH Zurich
// All rights reserved.
//
// Please, refer to the LICENSE file in the root directory.
// SPDX-License-Identifier: BSD-3-Clause

namespace stdalgos::detail {

template <typename> struct is_execution_property : std::false_type {};
template <typename P>
concept execution_property = is_execution_property<P>::value;

template <typename... Properties>
requires
    // clang-format off
    (execution_property<Properties> &&...)
    // clang-format on
    struct execution_properties {
  std::tuple<std::decay_t<Properties>...> properties;
};

template <typename... Properties> auto make_execution_properties(Properties &&...properties) {
  return execution_properties<std::decay_t<Properties>...>(
      {std::forward<Properties>(properties)...});
}

template <typename ExecutionPolicy>
concept execution_policy =
    std::same_as<std::decay_t<ExecutionPolicy>, std::execution::sequenced_policy> ||
    std::same_as<std::decay_t<ExecutionPolicy>, std::execution::unsequenced_policy> ||
    std::same_as<std::decay_t<ExecutionPolicy>, std::execution::parallel_policy> ||
    std::same_as<std::decay_t<ExecutionPolicy>, std::execution::parallel_unsequenced_policy>;

template <execution_policy ExecutionPolicy>
struct is_execution_property<ExecutionPolicy> : std::true_type {};

// TODO: This is an attempt at a lightweight property mechanism for
// schedulers. This can probably take many forms. The important thing is that
// they can be required on schedulers/parallel algorithm call sites.
//
// Open questions:
// - open or closed set of properties? (most likely open)
// - if open, should missing properties be errors or not?
struct with_execution_property_t {
  // All properties that have a customization for the given scheduler are
  // applied.
  template <stdexec::scheduler Scheduler, execution_property ExecutionProperty>
  requires
      // clang-format off
      (stdexec::tag_invocable<with_execution_property_t, Scheduler const &, ExecutionProperty>)
      // clang-format on
      auto
      operator()(Scheduler const &sched, ExecutionProperty &&exec_property) const {
    return stdexec::tag_invoke(with_execution_property_t{}, sched,
                               std::forward<ExecutionProperty>(exec_property));
  }

  // Custom properties that can't be applied to the given scheduler are ignored.
  // TODO: Is this wise? See prefer/require from the various properties
  // proposals.
  template <stdexec::scheduler Scheduler, execution_property ExecutionProperty>
  requires
      // clang-format off
      (!stdexec::tag_invocable<with_execution_property_t, Scheduler const &, ExecutionProperty>)
      // clang-format on
      decltype(auto)
      operator()(Scheduler &&sched, ExecutionProperty &&exec_property) const {
    return std::forward<Scheduler>(sched);
  }

  // TODO: Is an execution policy a required property of schedulers that are
  // passed to parallel algorithms?
  template <stdexec::scheduler Scheduler, execution_property ExecutionProperty0,
            execution_property... Rest>
  requires
      // clang-format off
      (!stdexec::tag_invocable<with_execution_property_t, Scheduler const &, ExecutionProperty0, Rest...>)
      // clang-format on
      auto
      operator()(stdexec::scheduler auto const &sched, ExecutionProperty0 &&exec_property0,
                 Rest &&...rest) const {
    return with_execution_property_t{}(
        with_execution_property_t{}(sched, std::forward<ExecutionProperty0>(exec_property0)),
        std::forward<Rest>(rest)...);
  };

  auto operator()(stdexec::scheduler auto const &sched) const { return sched; };
};

inline constexpr with_execution_property_t with_execution_property{};

// TODO: forwarding/moving/const/ref is not consistent.
template <stdexec::scheduler Scheduler, typename... Properties>
auto with_execution_properties(Scheduler const &sched,
                               execution_properties<Properties...> exec_properties) {
  return std::apply(
      [&](auto &&...p) { return with_execution_property(sched, std::forward<decltype(p)>(p)...); },
      std::move(exec_properties.properties));
}
} // namespace stdalgos::detail

# Free-form notes moved from `algorithm.hpp`

Requirements:
- must be able to use scheduler from environment (whether
  `get_completion_scheduler` or through some connect-mechanism)
- must be able to customize at the top-level (i.e. full algorithm) based on
  scheduler

TODO: Should the callable be sent by the sender or provided immediately?
Precedent: then takes f immediately, the value by sender.

TODO: Should shapes be sent by the sender or provided immediately?
Precedent: bulk takes the shape directly, additional data by sender

TODO: Where and how do `execution_policy` constraints get applied?

TODO: Are other hints/properties required besides `seq`/`par`/`par_unseq`?
algorithms/bulk:
- chunk size (HPX, Kokkos)
- light/heavy i.e. work size (Kokkos)
- schedule i.e. work-stealing, static distribution, round-robin etc. (HPX,
  Kokkos)

generic properties:
- priorities
- stack size
- affinity

TODO: Some schedulers can provide only maximum seq, no par. Other schedulers can
provide only `par`/`par_unseq`, no `seq` (is this true? can use various
fallbacks...). How would one recreate (i.e. get exactly the same behaviour)
`std::for_each(std::execution::par, b, e, f)` using the asynchronous overloads?

```
transfer_just(system_context().get_scheduler(), b, e) |
  stdalgos::for_each(std::execution::par, f)?

just(b, e) | stdalgos::for_each(std::execution::par, f)?

transfer_just(with_policy(system_context().get_scheduler(), std::execution::par), b, e) |
  stdalgos::for_each(f)?
```

Should `for_each` above be sequential? Probably yes. The policy on the scheduler
may not be visible to the writer at the `for_each` call. Or should the `par` set
the default and it can be overridden at the `for_each` callsite?

TODO: Should `seq`/`par`/`par_unseq` have a relation to forward progress
guarantees of a scheduler?

NOTE: Comparison to HPX's mechanism: HPX bundles all "properties" into the
execution policy for parallel algorithms, including whether or not a particular
invocation is asynchronous or not:
- `hpx::execution::par`: use parallel policy with default scheduler
- `par.on(executor/scheduler)`: use parallel policy on a particular executor
  (the executor advertises if it supports a given policy)
- `par(task).on(executor)`: the invocation will return a future
- `par(task).on(executor).with(chunk_size)`: apply a given chunk size to the
  invocation
- `par(task).on(with_priority/stacksize(executor)).with(chunk_size)`: apply a
  given priority/stacksize to the executor

Differences to what's below:
- synchronous/asynchronous execution is handled by the same(-looking) overloads
  in HPX. This should be separate overloads (potentially in different
  namespaces).
- the executor/scheduler is provided per-call in HPX. This should be provided
  with the same mechanism as all other P2300 algorithms
  (`get_completion_scheduler`/`connect`?).

NOTE: Comparison to Kokkos' mechanism: Kokkos also bundles all "properties" into
an execution policy, including where something runs. Kokkos does not have
fundamental support for asynchronous execution, though in practice it does
happen with some execution spaces (which requires fencing eventually; think
`async_scope` with start_detached followed by `sync_wait` on
`async_scope::empty`).  Kokkos also combines the iteration pattern into the
execution policy.
- `Kokkos::RangePolicy()`: default execution space
- `Kokkos::RangePolicy<Kokkos::Cuda>()`: a specific execution space
- `Kokkos::RangePolicy<Kokkos::Cuda>().set_chunk_size(chunk_size)`: use a given
  chunk size
- `require(Kokkos::RangePolicy<Kokkos::Cuda>(), hint)`: use a given hint

TODO: Let scheduler be provided from the environment/sender. Pack all other
properties, including the execution policy, into something else? This _must_ be
extensible so that one can add scheduler-specific properties. Requiring it be
extensible is one argument for just bundling it with the scheduler instead and
providing some sort of env from it.

An argument against bundling it with the scheduler is that execution policy,
chunk sizes etc. are call-specific (may depend on data, callable, iteration
range, etc.). However, the scheduler and/or tag_invoke customizations with the
scheduler must still at some point be allowed to access the data at some point.

Compromise: schedulers are fundamentally what hold properties, but they can be
overridden on a per-call basis.

TODO: What is the basis set? What are the fundamental overloads? This is
currently a bit too free-form...

synchronous overloads:
- existing: nothing
- existing: execution policy

- new: execution properties on their own? (apply to system scheduler?)
- new: scheduler? (basis)

asynchronous overloads:
- new: nothing (equivalent to passing get_completion_scheduler if one exists,
  otherwise system scheduler?)
- new: execution policy? (local override for the current algorithm only? special
  case of one single execution property, gets applied to
  get_completion_scheduler if exists, otherwise system scheduler?)
- new: execution properties? (local overrides for the current algorithm only?
  generalization of the above, gets applied to get_completion_scheduler if
  exists, otherwise system scheduler?)
- new: scheduler? (basis)

The following lists a possible customization hierarchy. `algo()` mark forwarding
calls to the algorithm itself. `[...]` mark customizations. This doesn't quite
make full sense yet.
- Does the order make sense?
- Are the system_scheduler fallbacks necessary?
- Are so many levels necessary? Can some levels be left out to keep things
  simple?
- Can the policy and properties overloads be collapsed (they're essentially the
  same)?
- Do the property/policy-less overloads default to seq or use some default from
  scheduler?
- Is the last overload necessary or should one prefer on(scheduler, algo(f))?
- Are customizations correctly taken into account with a mix of `on(...)` and
  `get_completion_scheduler`? Probably not. The below needs to change if P2300
  changes how schedulers are taken into account (through senders or receivers).

1. `algo(f, ...)` -> `algo(properties(), f, ...)` or `algo(system_scheduler, f,
   ...)` [not customizable]
2. `algo(properties, f, ...)` -> `algo(with(system_scheduler, properties), f,
   ...)` [not customizable]
3. [this is P2500] `algo(scheduler, f, ...)` -> [scheduler customization 1.] or
   `sync_wait(just(...) | on(scheduler, algo(f))` or
   `sync_wait(transfer_just(scheduler, ...) | algo(f))`
4. `algo(sender, f)` -> `algo(sender, properties(), f)`
5. `algo(sender, properties, f)` -> [`with(completion_scheduler(sender),
   properties) customization 2.`] or [sender customization 3.] or [default
   implementation: look at this carefully to make sure execution policy
   requirements are not violated]

The above exposes three customization points, which are used in this order
(though not all are applicable in all situations):
1. `tag_invoke(algo_t, scheduler, f, ...)` [blocking]
2. `tag_invoke(algo_t, scheduler, sender, f)` [asynchronous]
3. `tag_invoke(algo_t, sender, properties, f)` [asynchronous]

The following don't necessarily have to be specified, but it must be possible
to use the same principles for them in the future.
- Ranges overloads?
- std::linalg overloads?
- Delegation from one algorithm to another? Guarantee or allow? What about
  compile-times?

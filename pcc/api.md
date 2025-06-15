# Design Notes/Questions

###
Check PCC definitions

### PCM
1. We don't distinguish between sender/receiver sides.
2. **Datatypes:**
    - Now implementation supports only int and sometimes casts stuff to uint32_t to achieve compatibility with kernel
    - TODO: support uint32_t on api level.
    - TODO: support floats
2. **Last seq no call**
    - Do PDCs have seq nos (they must!!!)?
    - Signal - cwnd (oldest unacked packet)
    - seq no can be used to make decisions
3. **Indexes**
    - Should be the user index to lookup/set signal/control/local_state be known at handler compile time or it could be computed at runtime? To me it looks like most of the cases would work with indexes known at compile time. However, one use case with runtime index could be computing credit grant on receiver side (assuming that we have multiple priorities aka pull queues)
    - *Now we have it at compile time.*
4. **Constants**
    - Host-side might want to set constants (algo params) into read only on the PCMI side
5. **Signal semantics:** 
    - For the signals, not sure we want to have a generic `uet_set_signal(idx, value)` on the handler side. Looks like we implicitly assume that all signals start from zero (right after PCMI is instantiated) and can eventually increase upon some events are happening (hence they can be used as triggers upon *reaching* the threshold). Thus rather then having a set and allow user to set signal to an arbitrary current value (even above the threshold which makes no sense!), I'd have two calls: `uet_signal_reset(idx)` + `uet_signal_threshold(..)`?
    - Is it possible that signals can have diferent type of thresholding/triggering?
    - It should be possible for some signals (e.g., RTT) to be initialzed with non zero def (to support `uet_signal_reset` in this case, an additional logic would be needed).
    a. **Signal update call**
        - we defined new `update` call on signal to avoid losing events with `set(..)`
        - *possible bug* it takes int as an update argument: if signal datatype is uint32_t, int will cover only half
    - *TODO:* `signal_set_initial_value()` is missing
    - For timer/burst triggers we support the following semantics:
        - Handler can enable/disable trigger by setting a signal `set_signal()` to positibe/zero value, correspondingly.  
6. **User notification about trigger**
    - We might want to deliver to the index of signal that triggered handler execution
    - Fast way to demultiplex which trigger resulted in handler invocation
    - `get/set_signal()` are not enough to understand unless flow state stores a threshold or `get/set_threshold` is implemented
    - anyway, will need to spend some instructions instead of doing simple clean `switch` statement.
    - it's possible that handler will observe signal index with lower priority (e.g., if burst completion is always noticed before ECN) - signal trigger prios?
7. **Notes on implementing CCs**
    - Swift: we don't support pacer delay output and don't have FP cwnd defined
    - DCTCP: no load balancing, uses uint32_t to work with alpha, might be broken due to casts
    - DCQCN: uses floats local state

### Runtime
1. Can we generalize current pthread-based scheduler+flow-threads to the portable sofwtare C runtime SDK, such that it can be seamlessly integrated with HTSIM, DPA/BF or into libfabric (RxD)?
2. HTSIM integration
3. AMD-like state machine support

### Implementation notes:
1. Right now flow generator generates new events, while scheduler asynchronously checks whether thresholds are met and triggers new events. Is this optimal? Can flow generator (aka datapath) detect whether trigger criteria is met and add new flow into a scheduler's queue?
2. Is there a reliable way to ensure that scheduler wouldn't schedule a flow TWICE for the same trigger? E.g., handler didn't reset the signal...

## Matching to create CCC and PCMI creation
- We install a tuple of source-dest addresses {FEP, JobIB, PIDonFEP, RI, TC} such that any CCC matching it will have the PCMI attached.

- The tuple supports wildcarding in the source and/or dest tuples to support ranges of addresses, e.g., pick the same PCMI for all inter-DC CCCs and another one for all intra-DC CCCs – these wildcards could be bitmasks of the address tuples (could also select specific services for specific PCM instances).

- PCM supports a `uet_register_pcm(source addr, source mask, dest addr, dest mask, handle)` function that installs the PCMI on a FEP **MK:FEP->CCC?**, which returns a handle, that can be used to remove this PCMI later. It returns a handle that enables programmers to define signals and the PCM algorithm. After signals and algorithm are installed, the PCM instance is activated using `uet_activate_pcm(handle)`, and the removal would be `uet_deregister_pcm(handle)`.

## PCMI management
- The signal installation functions are:

- `uet_register_signal(signal_name, accumulation_op, signal_index, handle)` - this function returns:
    - `UET_SIGNAL_NOT_SUPPORTED`
    - `UET_ACCUMULATION_NOT_SUPPORTED`
    - `UET_ACCUMULATION_ON_SIGNAL_NOT_SUPPORTED`
    - `UET_INDEX_BUSY`
    - `UET_RESOURCE_ERROR` – signal_index must always start from `zero` and count to `num_signals-1`

- `uet_register_invoke_trigger(signal_index, threshold, handle)` - this function defines when to invoke the algorithm on a PCMI. An implementation may allow for multiple of such triggers but may also return `UET_RESOURCE_ERROR` (re-invoke the PCM algorithm on the current CCC after the signal value reaches or exceeds the threshold. If persistent is true, then the threshold will be set to the specified value after each invocation until persistent is set false. A threshold value of zero disables further invocations based on the specified signal.).

### Control management
- `uet_register_control(control_name, control_index, handle)` - this function defines control outputs and maps them to indices. It may return `UET_RESOURCE_ERROR`.

- `uet_register_contol_initial_value(control_index, initial_value, handle)` - sets the initial value for the control knob at `control_index`, it is reset whenever a CCC is created. If a `control_index` does not have an initial value, then it will use UET’s default.

### State management
- `uet_register_local_state_size(int bytes)`

### Algorithm compilation and installation
- `uet_compile_algorithm(source_code, return_string, uet_alg, handle)` – returns string with errors and warnings (like webGL).

    - *Advice to implementors:* one should record the reg_signal and invoke_trigger functions and compile the algorithm with the specified index-to-signal mapping, i.e., function calls to `uet_get_signal(...)` may be replaced by a simple load operation

    - *Advice to implementors:* one could cache the registered signals and algorithms in case the same ones are installed multiple times for different CCCs

- `uet_register_algorithm(uet_alg, handle)` – install a compiled algorithm for the specified PCM.
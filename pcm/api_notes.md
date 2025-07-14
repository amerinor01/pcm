# Design Notes/Questions

### PCM Notes
1. We don't distinguish between sender/receiver sides.
2. Flow/PDC mathing is out of the context
3. **Datatypes:**
    - All datatypes exposed to the user are represented as `pcm_<uint/int/float>`
        - expectation is that handlers are written such that there is no assumption made on the width of the datatype
4. **Last seq no call**
    - Do PDCs have seq nos (they must!!!)?
    - Signal - cwnd (oldest unacked packet)
    - seq no can be used to make decisions
5. **Indexes**
    - Should be the user index to lookup/set signal/control/local_state be known at handler compile time or it could be computed at runtime? To me it looks like most of the cases would work with indexes known at compile time. 
        - However, one use case with runtime index could be computing credit grant on receiver side (assuming that we have multiple priorities aka pull queues)
        - *For now we have it at the compile time - also required by compiler.*
        - Having many priorities can be handled with `swich` statement
    - With YAML-based API indexes will not be `int` but names (that are mapped to integers)
        - YAML-based compilation will generate corresponding enums/defs
6. **Constants**
    - Host-side might want to set constants (algo params) into read only on the PCMI side
    - We have constants now, but per-thread:
        - Should we introduce shared storage?
7. **Signal semantics:** 
    - For the signals, not sure we want to have a generic `uet_set_signal(idx, value)` on the handler side. Looks like we implicitly assume that all signals start from zero (right after PCMI is instantiated) and can eventually increase upon some events are happening (hence they can be used as triggers upon *reaching* the threshold). Thus rather then having a set and allow user to set signal to an arbitrary current value (even above the threshold which makes no sense!), I'd have two calls: `uet_signal_reset(idx)` + `uet_signal_threshold(..)`?
    - Is it possible that signals can have diferent type of thresholding/triggering?
    - It should be possible for some signals (e.g., RTT) to be initialzed with non zero def (to support `uet_signal_reset` in this case, an additional logic would be needed).
    - *TODO:* `signal_set_initial_value()` is missing
    - For timer/burst triggers we support the following semantics:
        - Handler can re-enable timer/byte counter by setting a signal `set_signal()` to `PCM_SIG_REARM`.
8. **User notification about trigger**
    - We might want to deliver to the index of signal that triggered handler execution
    - Fast way to demultiplex which trigger resulted in handler invocation
        - `get/set_signal()` are not enough to understand unless flow state stores a threshold or `get/set_threshold` is implemented
        - anyway, will need to spend some instructions instead of doing simple clean `switch` statement.
        - it's possible that handler will observe signal index with lower priority (e.g., if burst completion is always noticed before ECN) - signal trigger prios?
        - `get_signal_invoke_trigger_user_index()` was introduced in DCQCN
9. **Notes on implementing CCs**
    - TCP:
        - for now all signals are consumed 1-by-1
    - DCTCP:
        - no load balancing yet
    - Swift:
        - we don't support pacer delay output
        - we don't support FP cwnd that goes below 1
        - similarly to HTSIM, we only compute network-delay Swift part, but not remote host delay
    - DCQCN: uses floats local state, uses baseRTT*rate to convert rate to cwnd
        - Uses two timers (alpha + rate increase)
        - TODO: support version with a single timer
    - SMaRTT:
        - uses MSS size across all computations
        - no load balancing yet
    - TODOs:
        - We now have 5 CCs.
        - Having 5 more would be great (or at least CUBIC and BBR!)
        - Read UE spec regarding receiver-side EQDS/NDP logic
            - what can we program there?
            - How to map/express NDP/EQDS's control atop PCMs signals/triggers/controls
10. **Things to track in the UE PCM spec**
    - accessor-based API and not structs
    - `set_threshold` to change threshold/enable/disable timers
    - constants API.
    - `get_signal_invoke_trigger_user_index`

### Runtime TODOs

1. Can we generalize current pthread-based scheduler+flow-threads to the portable sofwtare C runtime SDK, such that it can be seamlessly integrated with HTSIM, DPA/BF or into libfabric (RxD)?
    - First prototype on BF2:
        - *Pros:*
            - we already have two servers and can start prototyping early at 100G, I'm familiar with Verbs API
            - we can do single-packet RC writes to emulate per-packet traffic (we need a way to disable NIC CC for RC)
        - *Cons:*
            - Most likely need to support our own packet pacer
            - BF-2 ARM is funky, and DPU-based design is costly
    - Prototype on DPA?
        - Need BF3/4/CX8
    - Prototype on Pensando?
        - Need Poolara 400? What are the programming capabilities?
    - Prototype on DPDK?
        - *Pros*
            - supported by ALL vendors
        - *Cons*
            - Offloading is only with DPUs
            - Memory bandwidth for staging
            - Need to integrate PCM into some stack
2. HTSIM integration
    - Done integration with spcl/HTSIM
    - TODO:
        - Port to the UE's htsim
        - ATLAHS/LGS

# OLD API ideas

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
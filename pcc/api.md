# PCM definitions
### Notes
1. We don't distinguish between sender/receiver sides.
2. Pick the same PCMI for all inter-DC CCCs and another one for all intra-DC CCCs
    - TH: I think we need to have one PCMI per CCC in any case - otherwise output controls don't work!
    - MK: Agree with TH's point, but at that point TBH I don't really see a difference between CCC and PCMI abstractions. In the end controls are unique to the CCC.
        - PDS<--->CCC<--->PCMI

    **Proposal to discuss:**
    1. Allocate and configure PCMC
        1. `uet_alloc_pcmc(pcmc_handle)` - alloc new PCMC
        2. `uet_register_signal(ctrl, sig_idx, pcmc_handle)` + `uet_register_signal_handler_trigger(threshold, sig_idx, pcmc_handle)` - add signals
        3. `uet_register_control(ctrl, ctrl_idx, pcmc_handle)` + `uet_register_control_initial_value(value, ctrl_idx, pcmc_handle)` - add controls
        4. `uet_register_algorithm(algo_source_code, compile_output, pcmc_handle)` - compile algo
    2. Allocate CCC matching rule
        1. `uet_alloc_ccc_matching_rule(<matching tuple>, ccc_mr_handle)` - specify how to match PDS to new CCCs
    3. Activate PCM
        1. `uet_activate_pcm(pcmc_handle, ccc_mr_handle, new_pcm)` - activate:now PCMI will be instantiated upon new CCC gets matched.

2. **Indexes**
    - Should be the user index to lookup/set signal/control/local_state be known at handler compile time or it could be computed at runtime? To me it looks like most of the cases would work with indexes known at compile time. However, one use case with runtime index could be computing credit grant on receiver side (assuming that we have multiple priorities aka pull queues)

3. **Signal semantics:** 
    - For the signals, not sure we want to have a generic `uet_set_signal(idx, value)` on the handler side. Looks like we implicitly assume that all signals start from zero (right after PCMI is instantiated) and can eventually increase upon some events are happening (hence they can be used as triggers upon *reaching* the threshold). Thus rather then having a set and allow user to set signal to an arbitrary current value (even above the threshold which makes no sense!), I'd have two calls: `uet_signal_reset(idx)` + `uet_signal_threshold(..)`?
    - Is it possible that signals can have diferent type of thresholding/triggering?
    - It should be possible for some signals (e.g., RTT) to be initialzed with non zero def (to support `uet_signal_reset` in this case, an additional logic would be needed).

3. Do we have races in the API flow (no)?:
    1. Install CCC/PCMI mathing rule [Instructs NIC to setup steering/matching rule upon the first PDS packet comes]
    2. Setup signals/controls [at that point CCC/PCMIs can already be allocated, but we somehow dynamically attach signals to them]
    3. Compile algorithm [... at that point we have triggers that are already setup and active PDSs]
    4. Register algorithm
    - Race[?]: What happens if new connection (aka PDS) gets created between 1/2, 1/3, 2/3 or 1,4?
    - Can we ensure that CCC matching is installed only after algorithm is fully configured (signals/controls are set, and it is registerd).
    - Looks like this is solved with `uet_activate_pcm`.
- We do not specify format of the `source_code` object in `uet_compile_algorithm` call. What if `source_code` contains bunch of C functions, one calls another. Should we define algorithm entry point, e.g., `__pcm_algorithm_main___ int main_fn()`?
    - Algorithm should be able to return error when it is executed, e.g., we should be able to track it through something like a PCM event queue?

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
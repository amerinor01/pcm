#include "impl.hpp"

using namespace pcm;

inline constexpr pcm_float kFpVar = 42.0;
inline constexpr pcm_int kIntVar = -42;
inline constexpr pcm_uint kUintVar = 42;

using DatapathSpec =
    std::tuple<ControlDesc<PCM_CTRL_CWND, 8192, 0>,
               SignalDesc<PCM_SIG_ACK, PCM_SIG_ACCUM_SUM, PCM_SIG_NO_TRIGGER, (pcm_uint{1} << 0)>,
               SignalDesc<PCM_SIG_RTO, PCM_SIG_ACCUM_LAST, 10, (pcm_uint{1} << 1)>,
               VariableDesc<pcm_float, kFpVar, 0>, VariableDesc<pcm_int, kIntVar, 1>,
               VariableDesc<pcm_uint, kUintVar, 2>>;

inline constexpr const char AlgoName[] = "dctcp";

using FlowSpecType = SimpleFlow<AlgoName, DatapathSpec>;

extern "C" FlowDesc* dummy_spec() {
    return new FlowSpecType{};  // Return pointer to concrete type
}

int main() {
    util::GetTimeFn get_time_source = []() -> pcm_uint {
        return static_cast<pcm_uint>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::high_resolution_clock::now().time_since_epoch())
                .count());
    };

    Device dev{get_time_source};

    // For dlsym usage - pass the factory function directly
    dev.add_flow_spec_factory(dummy_spec, 0xFF);

    auto &fdesc1 = dev.create_flow(0x12);
    auto &fdesc2 = dev.create_flow(0x13);
    auto &fdesc3 = dev.create_flow(0x14);
    fdesc1.invoke_cc_algorithm_on_trigger();
    fdesc2.invoke_cc_algorithm_on_trigger();
    fdesc3.invoke_cc_algorithm_on_trigger();
}
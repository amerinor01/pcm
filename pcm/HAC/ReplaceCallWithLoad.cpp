/*
 * This pass replaces all calls to the functions flow_signal_get, flow_local_state_int_get(), flow_control_get with loads.
 *
 * Where we need to load from depends on the device we are compiling for:
 *   * a device could memory map signals to fixed addresses e.g., maximum rtt
 *     always lives at 0xdeadbeef in our address space (where 0xdeadbeef is a
 *     hw constant)
 *   * a device could memory map signals needed by the handler to a fixed 
 *     address, i.e., if the handler defines if wants to use maximum rtt as
 *     signal 3, this signal will be mapped to 0xdeadbeef + 3 
 *   * a device could memory map signals needed by the handler differently
 *     for each flow, so we need to get the mapping from the context passed
 *     to algorithm_main.
 *   * Different flows might get a private snapshot of the signals or signals
 *     could be shared and we need to make sure reads/writes are atomic.
 *
 * The API can abstract all of this away (in our implementation the
 * backend / device / driver supplies function pointers for the getter
 * and setter functions).
 *
 * Ideally we have a compiler that can handle many cases, and the vendor
 * describes each product using a YAML file.
 *
 * For now we assume the signals are mapped to a different space for each flow
 * and are not private (different handler invocations can run on different
 * cores simultaneously) since a) this is the hardest case and b) this is how
 * the non-htsim backend works.
 *
 * The flow_t struct which is passed to alorithm_main contains a pointer to the
 * backend_ctx, which in turn contains a pointer to the signals array.
 * */


#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Pass.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"

// the function we are optimizing (algorithm_main(...)) gets multiple arguments
// we don't touch the first one (ctx) but need to know the positions of the
// others. In case their position changes, adapt the constants below.
const int signals_pos = 1;
const int controls_pos = 2;
const int thresholds_pos = 3;
const int local_state_pos = 4;
const int constants_pos = 5;

using namespace llvm;

namespace {

struct ReplaceCallWithLoadPass : public PassInfoMixin<ReplaceCallWithLoadPass> {
    PreservedAnalyses run(Function &F, FunctionAnalysisManager &) {
        LLVMContext &Ctx = F.getContext();
        bool Changed = false;

        for (auto &BB : F) {
            for (auto It = BB.begin(); It != BB.end(); ) {
                Instruction *I = &*It++;
                
                if (auto *Call = dyn_cast<CallInst>(I)) {
                    Function *Callee = Call->getCalledFunction();
                    if (!Callee) continue;

                    // Check if the called function is the one we want to replace
                    if (Callee->getName().endswith("flow_signal_get")) {
                        IRBuilder<> Builder(Call);
                        Type *RetType = Call->getType(); //we are inferring here the type of a signal - if the original code was wrong we will be fucked
			Type *RetTypePtr = RetType->getPointerTo();
                        Argument* Signals = std::next(F.arg_begin(), signals_pos); //the signals void* we need to cast and offset
                        Value *CastPtr = Builder.CreateBitCast(Signals, RetTypePtr); //cast signals to int32* or int64*
                        Value *Offset = Call->getArgOperand(1); //0th arg is the ctx, 1st the offset we need
                        Value *S = Builder.CreateInBoundsGEP(RetType, CastPtr, Offset); //get *signals[offset]
                        Value *LoadedVal = Builder.CreateLoad(RetType, S);
                        Call->replaceAllUsesWith(LoadedVal);
                        Call->eraseFromParent();
                        Changed = true;
                    }

                }
            }
        }

        return Changed ? PreservedAnalyses::none() : PreservedAnalyses::all();
    }
};

} // namespace

// Register the pass (if using opt tool)
extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo llvmGetPassPluginInfo() {
    return {
        LLVM_PLUGIN_API_VERSION, "ReplaceCallWithLoadPass", "v0.1",
        [](PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [](StringRef Name, FunctionPassManager &FPM,
                   ArrayRef<PassBuilder::PipelineElement>) {
                    if (Name == "replace-call-with-load") {
                        FPM.addPass(ReplaceCallWithLoadPass());
                        return true;
                    }
                    return false;
                });
        }
    };
}


#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Pass.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"

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
                    if (Callee->getName() == "external_func") {
                        IRBuilder<> Builder(Call);

                        // Assume return type is i32
                        Type *RetType = Call->getType();
                        Constant *AddrInt = ConstantInt::get(Type::getInt64Ty(Ctx), 0x12345678);
                        Value *Ptr = Builder.CreateIntToPtr(AddrInt, RetType->getPointerTo());
                        Value *LoadedVal = Builder.CreateLoad(RetType, Ptr);

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


#include "llvm/IR/PassManager.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/Dominators.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"

#include <vector>
#include <set>

using namespace llvm;

namespace {

class PathEnumeratorPass : public PassInfoMixin<PathEnumeratorPass> {
public:
    PreservedAnalyses run(Function &F, FunctionAnalysisManager &FAM) {
        //if (F.getName() != "foo")
        //    return PreservedAnalyses::all();

        outs() << "Enumerating paths in function: " << F.getName() << "\n";

        DominatorTree &DT = FAM.getResult<DominatorTreeAnalysis>(F);
        LoopInfo &LI = FAM.getResult<LoopAnalysis>(F);

        std::set<const BasicBlock *> backEdges;
        for (Loop *L : LI) {
            for (auto *BB : L->blocks()) {
                for (auto *Succ : successors(BB)) {
                    if (L->contains(Succ)) {
                        backEdges.insert(Succ);
                    }
                }
            }
        }

        std::vector<std::vector<const BasicBlock *>> allPaths;
        std::vector<const BasicBlock *> currentPath;
        std::set<const BasicBlock *> visited;

        auto dfs = [&](auto &&dfs, const BasicBlock *BB) -> void {
            currentPath.push_back(BB);
            visited.insert(BB);

            if (succ_empty(BB)) {
                allPaths.push_back(currentPath);
            } else {
                for (const BasicBlock *Succ : successors(BB)) {
                    if (visited.count(Succ)) {
                        allPaths.push_back(currentPath); // backedge encountered
                        continue;
                    }
                    dfs(dfs, Succ);
                }
            }

            currentPath.pop_back();
            visited.erase(BB);
        };

        dfs(dfs, &F.getEntryBlock());

        for (const auto &path : allPaths) {
            bool hasLoop = false;
            std::set<const BasicBlock *> seen;
            unsigned instrCount = 0;

            for (const BasicBlock *BB : path) {
                if (seen.count(BB)) {
                    hasLoop = true;
                    break;
                }
                seen.insert(BB);
                instrCount += BB->size();
            }

            errs() << "Path: ";
            for (const BasicBlock *BB : path)
                errs() << BB << " -> ";
            errs() << (hasLoop ? "Inf" : std::to_string(instrCount)) << "\n";
        }

        return PreservedAnalyses::all();
    }
};

} // namespace

/// Register pass with the plugin interface
extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo llvmGetPassPluginInfo() {
    return {
        LLVM_PLUGIN_API_VERSION, "PathEnumeratorPass", LLVM_VERSION_STRING,
        [](PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [](StringRef Name, FunctionPassManager &FPM,
                   ArrayRef<PassBuilder::PipelineElement>) {
                    if (Name == "enumerate-paths") {
                        FPM.addPass(PathEnumeratorPass());
                        return true;
                    }
                    return false;
                });
        }};
}


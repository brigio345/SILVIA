#include "llvm/ADT/SmallVector.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/XILINXLoopInfoUtils.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Value.h"
#include "llvm/Pass.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Utils/XILINXLoopUtils.h"
#include <string>

#define INSERT_DUMMY_DB_DEBUG_
using namespace llvm;

namespace {
static cl::opt<std::string>
    BBFnName("insert-dummy-bb-fn", cl::ValueRequired,
             cl::desc("The name of the blackbox function to call."));
static cl::opt<std::string>
    BBTopName("insert-dummy-bb-top", cl::ValueRequired,
              cl::desc("The name function where to insert the blackbox call."));

const std::string BEGIN_SEP(std::string("\n\n") + std::string(80, '>') +
                            std::string("\n\n"));
const std::string END_SEP(std::string("\n\n") + std::string(80, '<') +
                          std::string("\n\n"));

struct InsertDummyBB : public ModulePass {
  static char ID;
  InsertDummyBB() : ModulePass(ID) {}

  bool runOnModule(Module &M) override;
  bool doInitialization(Module &M) override;
  bool doFinalization(Module &M) override;
  void getAnalysisUsage(AnalysisUsage &AU) const override {
    // AU.setPreservesCFG();
    AU.addRequired<LoopInfoWrapperPass>();
  }

private:
  StringRef _getBBName(Module &M) const;
  std::vector<Value *> _getBBFnArgs(Function *F) const;
  bool _isBBCalled(Module &M, StringRef &BBName) const;
};

StringRef InsertDummyBB::_getBBName(Module &M) const {
  StringRef BBName("");
  for (Function &F : M) {
    if (F.empty())
      continue;
    if (std::string(F.getName()).find(BBFnName) != std::string::npos) {
      BBName = F.getName();
#ifdef INSERT_DUMMY_DB_DEBUG_
      dbgs() << BEGIN_SEP << "Found the BB declaration: " << F.getName()
             << END_SEP;
#endif // INSERT_DUMMY_DB_DEBUG_
    }
  }
  return BBName;
}

bool InsertDummyBB::_isBBCalled(Module &M, StringRef &BBName) const {
  for (auto &F : M) {      // iterate over all functions in the module
    for (auto &BB : F) {   // iterate over all basic blocks in the function
      for (auto &I : BB) { // iterate over all instructions in the basic block
        if (auto *callInst = dyn_cast<CallInst>(
                &I)) { // if the instruction is a call instruction
          if (auto *calledFunction =
                  callInst->getCalledFunction()) { // get the called function
            if (calledFunction->getName() ==
                BBName) { // check if the called function is the one we
                          // are looking for
              return true;
            }
          }
        }
      }
    }
  }
  return false;
}

bool InsertDummyBB::doInitialization(Module &M) { return false; }

bool InsertDummyBB::doFinalization(Module &M) { return false; }

bool InsertDummyBB::runOnModule(Module &M) {
  // Iterating over the functions available in the module.
  bool finished = false;

  auto BBName = _getBBName(M);
  if (BBName == "") {
    dbgs() << "ERROR: Could not find the BB function in the module. The fake"
              "call will not be inserted.\n";
    return false;
  }

  if (_isBBCalled(M, BBName)) {
#ifdef INSERT_DUMMY_DB_DEBUG_
    dbgs() << BEGIN_SEP << "There is already a call to the BB." << END_SEP;
#endif // INSERT_DUMMY_DB_DEBUG_
    return false;
  }

  auto BBFn = M.getFunction(BBName);
  if (!BBFn) {
    dbgs() << "ERROR: Could not look up the function.\n";
    return false;
  }

  for (Function &F : M) {
    if (F.getName() != BBTopName || F.empty() || F.isDeclaration())
      continue;

    LLVMContext &context = F.getContext();

    BasicBlock *originalEntry = &F.getEntryBlock();

    BasicBlock *newBB =
        BasicBlock::Create(context, "dummy", &F, &F.getEntryBlock());

    // FIXME: loop should be freed (?)
    Loop *loop = new Loop();
    LoopInfo *LI = &getAnalysis<LoopInfoWrapperPass>(F).getLoopInfo();
    loop->addBasicBlockToLoop(newBB, *LI);

    IRBuilder<> builder(context);
    builder.SetInsertPoint(newBB);
    SmallVector<Value *, 8> args;
    args.push_back(builder.CreateAlloca(
        BBFn->getArg(0)->getType()->getPointerElementType()));
    for (int i = 1; i < BBFn->arg_size(); i++)
      args.push_back(Constant::getNullValue(BBFn->getArg(i)->getType()));

    // FIXME: do not set a random debug location just to generate valid IR.
    auto call = builder.CreateCall(BBFn, args);
    DebugLoc DL;
    for (auto &BB : F) {
      for (auto &I : BB) {
        auto IDL = I.getDebugLoc();
        if (IDL) {
          DL = IDL;
          break;
        }
      }
      if (DL)
        break;
    }
    call->setDebugLoc(DL);

    builder.CreateBr(originalEntry);

    LI->addTopLevelLoop(loop);

    addPipeline(loop);
    finished = true;

    break;
  }
#ifdef INSERT_DUMMY_DB_DEBUG_
  dbgs() << std::string(80, '-') << "\n\nExiting the pass...\n\n"
         << std::string(80, '-') << "\n";
#endif // INSERT_DUMMY_DB_DEBUG_
  return finished;
}
} // namespace

char InsertDummyBB::ID = 0;
static RegisterPass<InsertDummyBB>
    X("insert_dummy_bb", "Inserts a dummy blackbox call to make Vitis happy",
      false /* Only looks at CFG */, false /* Transformation Pass */);

#include "llvm/ADT/SmallVector.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Value.h"
#include "llvm/Pass.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Utils/XILINXFunctionUtils.h"
#include <string>

#define CALL_BLACK_BOX_DEBUG_
using namespace llvm;

namespace {
static cl::opt<std::string>
    BlackBoxFnName("call-black-box-fn", cl::ValueRequired,
                   cl::desc("The name of the blackbox function to call."));
static cl::opt<std::string> BlackBoxTopName(
    "call-black-box-top", cl::ValueRequired,
    cl::desc("The name function where to insert the blackbox call."));

const std::string BEGIN_SEP(std::string("\n\n") + std::string(80, '>') +
                            std::string("\n\n"));
const std::string END_SEP(std::string("\n\n") + std::string(80, '<') +
                          std::string("\n\n"));

struct CallBlackBox : public ModulePass {
  static char ID;
  CallBlackBox() : ModulePass(ID) {}

  bool runOnModule(Module &M) override;
  bool doInitialization(Module &M) override;
  bool doFinalization(Module &M) override;
  void getAnalysisUsage(AnalysisUsage &AU) const override {
    // AU.setPreservesCFG();
    AU.addRequired<LoopInfoWrapperPass>();
  }

private:
  StringRef _getBlackBoxName(Module &M) const;
  std::vector<Value *> _getBlackBoxFnArgs(Function *F) const;
  bool _isBlackBoxCalled(Module &M, StringRef &BlackBoxName) const;
};

StringRef CallBlackBox::_getBlackBoxName(Module &M) const {
  StringRef BlackBoxName("");
  for (Function &F : M) {
    if (F.empty())
      continue;
    if (std::string(F.getName()).find(BlackBoxFnName) != std::string::npos) {
      BlackBoxName = F.getName();
#ifdef CALL_BLACK_BOX_DEBUG_
      dbgs() << BEGIN_SEP << "Found the BlackBox declaration: " << F.getName()
             << END_SEP;
#endif // CALL_BLACK_BOX_DEBUG_
    }
  }
  return BlackBoxName;
}

bool CallBlackBox::_isBlackBoxCalled(Module &M, StringRef &BlackBoxName) const {
  for (auto &F : M) {      // iterate over all functions in the module
    for (auto &BB : F) {   // iterate over all basic blocks in the function
      for (auto &I : BB) { // iterate over all instructions in the basic block
        if (auto *callInst = dyn_cast<CallInst>(
                &I)) { // if the instruction is a call instruction
          if (auto *calledFunction =
                  callInst->getCalledFunction()) { // get the called function
            if (calledFunction->getName() ==
                BlackBoxName) { // check if the called function is the one we
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

bool CallBlackBox::doInitialization(Module &M) { return false; }

bool CallBlackBox::doFinalization(Module &M) { return false; }

// Recursively build a constant struct, assuming that the innermost elements
// are integers.
Constant *getConstant(Type *argType, int value) {
  auto argIntType = dyn_cast<IntegerType>(argType);
  if (argIntType)
    return ConstantInt::get(argIntType, value);

  auto argStructType = dyn_cast<StructType>(argType);
  if (!argStructType)
    return nullptr;

  SmallVector<Constant *, 1> constants;
  for (auto i = 0; i < argStructType->getNumElements(); ++i)
    constants.push_back(getConstant(argStructType->getElementType(i), value));
  return ConstantStruct::get(argStructType, constants);
}

bool CallBlackBox::runOnModule(Module &M) {
  // Iterating over the functions available in the module.
  bool finished = false;

  auto BlackBoxName = _getBlackBoxName(M);
  if (BlackBoxName == "") {
    dbgs()
        << "ERROR: Could not find the BlackBox function in the module. The fake"
           "call will not be inserted.\n";
    return false;
  }

  if (_isBlackBoxCalled(M, BlackBoxName)) {
#ifdef CALL_BLACK_BOX_DEBUG_
    dbgs() << BEGIN_SEP << "There is already a call to the BlackBox."
           << END_SEP;
#endif // CALL_BLACK_BOX_DEBUG_
    return false;
  }

  auto BlackBoxFn = M.getFunction(BlackBoxName);
  if (!BlackBoxFn) {
    dbgs() << "ERROR: Could not find function" << BlackBoxName << ".\n";
    return false;
  }

  for (Function &F : M) {
    if ((std::string(F.getName()).find(BlackBoxTopName) == std::string::npos) ||
        F.empty() || F.isDeclaration())
      continue;

    LLVMContext &context = F.getContext();

    auto blackBoxWrapper = Function::Create(
        FunctionType::get(Type::getVoidTy(context), {}, false),
        Function::ExternalLinkage, BlackBoxName + "_wrapper", &M);
    auto bb = BasicBlock::Create(context, "entry", blackBoxWrapper);
    IRBuilder<> builder(context);
    builder.SetInsertPoint(bb);

    SmallVector<Value *, 8> args;
    args.push_back(builder.CreateAlloca(
        BlackBoxFn->getArg(0)->getType()->getPointerElementType()));
    for (int i = 1; i < BlackBoxFn->arg_size(); i++) {
      auto argPointerType =
          dyn_cast<PointerType>(BlackBoxFn->getArg(i)->getType());
      auto argStructType =
          dyn_cast<StructType>(argPointerType->getElementType());
      auto argConstant = getConstant(argStructType, i);

      auto argGlobalVar = new GlobalVariable(
          M, argStructType, true, GlobalValue::LinkageTypes::PrivateLinkage,
          argConstant);
      args.push_back(argGlobalVar);
    }

    // FIXME: do not set a random debug location just to generate valid IR.
    auto call = builder.CreateCall(BlackBoxFn, args);
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
    builder.CreateRetVoid();

    addPipeline(blackBoxWrapper);
    auto entryBB = &F.getEntryBlock();
    builder.SetInsertPoint(entryBB);
    auto c = builder.CreateCall(blackBoxWrapper);
    c->moveBefore(entryBB->getFirstNonPHI());

    finished = true;

    break;
  }
#ifdef CALL_BLACK_BOX_DEBUG_
  dbgs() << std::string(80, '-') << "\n\nExiting the pass...\n\n"
         << std::string(80, '-') << "\n";
#endif // CALL_BLACK_BOX_DEBUG_
  return finished;
}
} // namespace

char CallBlackBox::ID = 0;
static RegisterPass<CallBlackBox>
    X("call-black-box", "Inserts a dummy blackbox call to make Vitis happy",
      false /* Only looks at CFG */, false /* Transformation Pass */);

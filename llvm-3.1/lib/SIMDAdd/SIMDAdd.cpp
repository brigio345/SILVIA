#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/BasicBlock.h"
#include "llvm/Function.h"
#include "llvm/Instructions.h"
#include "llvm/Module.h"
#include "llvm/Pass.h"
#include "llvm/Support/IRBuilder.h"

#include <queue>

using namespace llvm;

struct SIMDAdd : public BasicBlockPass {
  static char ID;
  SIMDAdd() : BasicBlockPass(ID) {}

  bool runOnBasicBlock(BasicBlock &BB) override;
};

char SIMDAdd::ID = 0;
static RegisterPass<SIMDAdd> X("simd-add", "Map add instructions to SIMD DSPs",
                               false /* Only looks at CFG */,
                               true /* Transformation Pass */);

Instruction *getLastOperandDef(Instruction *inst,
                               DenseMap<Instruction *, int> &instMap) {
  Instruction *lastDef = nullptr;

  for (unsigned i = 0; i < inst->getNumOperands(); i++) {
    Value *op = inst->getOperand(i);
    if (auto opInst = dyn_cast<Instruction>(op)) {
      if ((!lastDef) || (instMap[lastDef] < instMap[opInst]))
        lastDef = opInst;
    }
  }

  return lastDef;
}

Instruction *getFirstValueUse(Instruction *inst,
                              DenseMap<Instruction *, int> &instMap) {
  Instruction *firstUse = nullptr;

  for (auto UI = inst->use_begin(), UE = inst->use_end(); UI != UE; ++UI) {
    Value *user = *UI;
    if (auto userInst = dyn_cast<Instruction>(user)) {
      if ((!firstUse) || (instMap[userInst] < instMap[firstUse]))
        firstUse = userInst;
    }
  }

  return firstUse;
}

// Collect all the add instructions.
void getSIMDableInstructions(
    BasicBlock &BB,
    std::queue<SmallVector<Instruction *, 1>> &simd4Candidates) {
  for (auto &I : BB) {
    if (I.getOpcode() == Instruction::Add) {
      if (cast<IntegerType>(I.getType())->getBitWidth() <= 12)
        simd4Candidates.push(SmallVector<Instruction *, 1>{1, &I});
      // TODO: collect candidates for simd2
      // else if (cast<IntegerType>(binOp->getType())->getBitWidth() <= 24)
      // simd2Candidates.push_back(binOp);
    }
  }
}

void replaceAddsWithSIMDCall(
    SmallVector<SmallVector<Instruction *, 1>, 4> instTuple,
    Instruction *insertBefore, Function *myAddFunc, LLVMContext &context) {
  IRBuilder<> builder(insertBefore);

  Value *args[2];
  for (unsigned j = 0; j < 2; j++) {
    for (unsigned i = 0; i < instTuple.size(); i++) {
      Value *arg = builder.CreateZExt(instTuple[i][0]->getOperand(j),
                                      IntegerType::get(context, 48));
      int shift_amount = (12 * (3 - i));
      if (shift_amount > 0) {
        arg = builder.CreateShl(
            builder.CreateZExt(arg, IntegerType::get(context, 48)),
            shift_amount);
      }
      args[j] = (i > 0) ? builder.CreateOr(args[j], arg) : arg;
    }
  }

  Value *sum_concat = builder.CreateCall(myAddFunc, args);

  Value *result[4];
  for (unsigned i = 0; i < instTuple.size(); i++) {
    int shift_amount = (12 * (3 - i));
    Value *result_shifted = (shift_amount > 0)
                                ? builder.CreateLShr(sum_concat, shift_amount)
                                : sum_concat;

    result[i] = builder.CreateTrunc(
        result_shifted, instTuple[i][instTuple[i].size() - 1]->getType());
  }

  // Replace the add instruction with the result
  for (unsigned i = 0; i < instTuple.size(); i++) {
    instTuple[i][instTuple[i].size() - 1]->replaceAllUsesWith(result[i]);
    instTuple[i][instTuple[i].size() - 1]->eraseFromParent();
  }
}

bool SIMDAdd::runOnBasicBlock(BasicBlock &BB) {
  // FIXME: check if LLVM 3.1 provides alternatives to skipFunction
  // if (skipFunction(F))
  Function *F = BB.getParent();
  if (F->getName().startswith("_ssdm_op") ||
      F->getName() == "dsp_add_4simd_pipe_l0")
    return false;

  // Get the SIMD function
  Module *module = F->getParent();
  Function *myAddFunc = module->getFunction("dsp_add_4simd_pipe_l0");
  if (!myAddFunc)
    throw "Function dsp_add_4simd_pipe_l0 not found";

  LLVMContext &context = F->getContext();

  bool modified = false;

  std::queue<SmallVector<Instruction *, 1>> simd4Candidates;
  getSIMDableInstructions(BB, simd4Candidates);

  // TODO: Maybe use DominatorTree? (It may be an overkill)
  // DominatorTree DT(F);
  // if (DT.dominates(inst0, inst1)) {...}
  DenseMap<Instruction *, int> instMap;
  for (auto &inst : BB)
    instMap[&inst] = instMap.size();

  // Build tuples of 4 instructions that can be mapped to the
  // same SIMD DSP.
  // TODO: check if a size of 8 is a good choice
  while (!simd4Candidates.empty()) {
    SmallVector<SmallVector<Instruction *, 1>, 4> instTuple;
    Instruction *lastDef = nullptr;
    Instruction *firstUse = nullptr;

    for (unsigned i = 0; i < simd4Candidates.size(); i++) {
      Instruction *addInstCurr = simd4Candidates.front()[0];

      Instruction *lastDefCurr = getLastOperandDef(addInstCurr, instMap);
      Instruction *firstUseCurr = getFirstValueUse(addInstCurr, instMap);

      // Update with tuple worst case
      if ((!lastDef) ||
          (lastDefCurr && (instMap[lastDef] < instMap[lastDefCurr])))
        lastDef = lastDefCurr;

      if ((!firstUse) ||
          (firstUseCurr && (instMap[firstUseCurr] < instMap[firstUse])))
        firstUse = firstUseCurr;

      // If firstUse is before lastDef this pair of
      // instructions is not compatible with current
      // tuple.
      if (firstUse && (instMap[firstUse] < instMap[lastDef]))
        continue;

      simd4Candidates.pop();
      instTuple.push_back(SmallVector<Instruction *, 1>{1, addInstCurr});

      if (instTuple.size() == 4)
        break;
    }

    // TODO: maybe also skip tuples of size 2?
    if (instTuple.size() < 2)
      continue;

    replaceAddsWithSIMDCall(instTuple, firstUse, myAddFunc, context);
    modified = true;
  }

  return modified;
}

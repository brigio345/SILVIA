#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/Analysis/MemoryLocation.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"
#include "llvm/Support/CommandLine.h"

#include <cassert>
#include <list>

using namespace llvm;

struct SIMDAdd : public BasicBlockPass {
  static char ID;
  SIMDAdd() : BasicBlockPass(ID) {}

  bool runOnBasicBlock(BasicBlock &BB) override;

  virtual void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.addRequired<AAResultsWrapperPass>();
  }

  void anticipateDefs(Instruction *inst, bool anticipateInst);
  void posticipateUses(Instruction *inst, bool posticipateInst);
  bool isMoveMemSafe(Instruction *instToMove, Instruction *firstInst,
                     Instruction *lastInst);

  AliasAnalysis *AA;
};

struct CandidateInst {
  SmallVector<Instruction *, 2> inInsts;
  SmallVector<Instruction *, 1> outInsts;
};

char SIMDAdd::ID = 0;
static RegisterPass<SIMDAdd> X("simd-add", "Map add instructions to SIMD DSPs",
                               false /* Only looks at CFG */,
                               true /* Transformation Pass */);

static cl::opt<std::string>
    SIMDOp("simd-add-op", cl::init("add4simd"), cl::Hidden,
           cl::desc("The operation to map to SIMD DSPs. "
                    "Possible values are: add4simd."));

bool dependsOn(Instruction *inst0, Instruction *inst1) {
  if (inst0->getParent() != inst1->getParent())
    return false;

  if (inst0 == inst1)
    return true;

  for (int i = 0; i < inst0->getNumOperands(); ++i) {
    if (auto operandInst = dyn_cast<Instruction>(inst0->getOperand(i))) {
      if (dependsOn(operandInst, inst1))
        return true;
    }
  }

  return false;
}

void getInstMap(BasicBlock *BB, DenseMap<Instruction *, int> &instMap) {
  for (auto &inst : *BB)
    instMap[&inst] = instMap.size();
}

Instruction *getLastOperandDef(Instruction *inst) {
  BasicBlock *instBB = inst->getParent();

  DenseMap<Instruction *, int> instMap;
  getInstMap(instBB, instMap);

  Instruction *lastDef = nullptr;
  for (unsigned i = 0; i < inst->getNumOperands(); ++i) {
    Value *op = inst->getOperand(i);
    auto opInst = dyn_cast<Instruction>(op);
    if ((!opInst) || (opInst->getParent() != instBB))
      continue;
    if ((!lastDef) || (instMap[lastDef] < instMap[opInst]))
      lastDef = opInst;
  }

  return lastDef;
}

Instruction *getFirstValueUse(Instruction *inst) {
  BasicBlock *instBB = inst->getParent();

  DenseMap<Instruction *, int> instMap;
  getInstMap(instBB, instMap);

  Instruction *firstUse = nullptr;
  for (auto UI = inst->use_begin(), UE = inst->use_end(); UI != UE; ++UI) {
    Value *user = *UI;
    auto userInst = dyn_cast<Instruction>(user);
    if ((!userInst) || (userInst->getParent() != instBB))
      continue;
    if ((!firstUse) || (instMap[userInst] < instMap[firstUse]))
      firstUse = userInst;
  }

  return firstUse;
}

bool SIMDAdd::isMoveMemSafe(Instruction *instToMove, Instruction *firstInst,
                            Instruction *lastInst) {
  auto loadToMove = dyn_cast<LoadInst>(instToMove);
  auto storeToMove = dyn_cast<StoreInst>(instToMove);

  if ((!loadToMove) && (!storeToMove))
    return true;

  MemoryLocation locToMove = (storeToMove ? MemoryLocation::get(storeToMove)
                                          : MemoryLocation::get(loadToMove));

  bool toCheck = false;
  for (auto &I : *(instToMove->getParent())) {
    // Skip the instructions before the interval involved in the movement.
    if ((!toCheck) && (&I != firstInst))
      continue;
    toCheck = true;

    // Skip the instructions after the interval involved in the movement.
    if (&I == lastInst)
      break;

    if (auto store = dyn_cast<StoreInst>(&I)) {
      auto loc = MemoryLocation::get(store);

      if (AA->alias(locToMove, loc) != AliasResult::NoAlias)
        return false;
    }

    // If a load aliases with another load is not an issue. There is no need to
    // check.
    if (loadToMove)
      continue;

    if (auto load = dyn_cast<LoadInst>(&I)) {
      auto loc = MemoryLocation::get(load);

      if (AA->alias(locToMove, loc) != AliasResult::NoAlias)
        return false;
    }
  }

  return true;
}

void SIMDAdd::anticipateDefs(Instruction *inst, bool anticipateInst = false) {
  BasicBlock *instBB = inst->getParent();
  for (unsigned i = 0; i < inst->getNumOperands(); ++i) {
    Value *op = inst->getOperand(i);
    auto opInst = dyn_cast<Instruction>(op);
    if (!opInst)
      continue;
    if (opInst->getParent() == instBB)
      anticipateDefs(opInst, true);
  }

  if (!anticipateInst)
    return;

  Instruction *insertionPoint = instBB->getFirstNonPHI();
  auto lastDef = getLastOperandDef(inst);
  if (lastDef)
    insertionPoint = lastDef->getNextNode();

  if (!isMoveMemSafe(inst, insertionPoint, inst))
    return;

  inst->moveBefore(insertionPoint);
}

void SIMDAdd::posticipateUses(Instruction *inst, bool posticipateInst = false) {
  BasicBlock *instBB = inst->getParent();
  for (auto UI = inst->use_begin(), UE = inst->use_end(); UI != UE; ++UI) {
    Value *user = *UI;
    auto userInst = dyn_cast<Instruction>(user);
    if (!userInst)
      continue;
    if (userInst->getParent() == instBB)
      posticipateUses(userInst, true);
  }

  if (!posticipateInst)
    return;

  Instruction *insertionPoint = getFirstValueUse(inst);
  if (!insertionPoint)
    insertionPoint = instBB->getTerminator();

  if (!isMoveMemSafe(inst, inst, insertionPoint))
    return;

  inst->moveBefore(insertionPoint);
}

// Collect all the add instructions.
void getSIMDableInstructions(BasicBlock &BB,
                             std::list<CandidateInst> &candidateInsts) {
  for (auto &I : BB) {
    if (I.getOpcode() != Instruction::Add)
      continue;
    if (cast<IntegerType>(I.getType())->getBitWidth() <= 12) {
      CandidateInst candidate;
      candidate.inInsts.push_back(&I);
      candidate.outInsts.push_back(&I);
      candidateInsts.push_back(candidate);
    }
    // TODO: collect candidates for simd2
    // else if (cast<IntegerType>(binOp->getType())->getBitWidth() <= 24)
    // simd2Candidates.push_back(binOp);
  }
}

void replaceInstsWithSIMDCall(SmallVector<CandidateInst, 4> instTuple,
                              Instruction *insertBefore, Function *SIMDFunc,
                              LLVMContext &context) {
  // TODO: Select the pipelined or non-pipelined version of SIMDFunc based on
  // `isPipeline` of current loop and function.
  IRBuilder<> builder(insertBefore);

  Value *args[2] = {nullptr};
  std::string argName[2] = {""};
  std::string retName = "";
  for (unsigned i = 0; i < instTuple.size(); ++i) {
    retName = instTuple[i].outInsts[0]->getName().str() +
              std::string((i > 0) ? "_" : "") + retName;
    for (unsigned j = 0; j < instTuple[i].inInsts[0]->getNumOperands(); ++j) {
      auto operand = instTuple[i].inInsts[0]->getOperand(j);
      auto arg = builder.CreateZExt(operand, IntegerType::get(context, 48),
                                    operand->getName() + "_zext");
      int shift_amount = (12 * i);
      if (shift_amount > 0) {
        arg = builder.CreateShl(arg, shift_amount, operand->getName() + "_shl");
      }

      argName[j] = operand->getName().str() + std::string((i > 0) ? "_" : "") +
                   argName[j];
      if (args[j])
        arg = builder.CreateOr(args[j], arg, argName[j]);

      args[j] = arg;
    }
  }

  Value *sum_concat = builder.CreateCall(SIMDFunc, args, retName);

  Value *result[4];
  for (int i = 0; i < instTuple.size(); ++i) {
    int shift_amount = (12 * i);

    std::string instName = "";
    for (int j = (instTuple.size() - 1); j >= i; --j)
      instName += instTuple[j].outInsts[0]->getName().str() + "_";
    instName += "sext";

    Value *result_shifted =
        (shift_amount > 0)
            ? builder.CreateLShr(sum_concat, shift_amount, instName)
            : sum_concat;

    result[i] =
        builder.CreateTrunc(result_shifted, instTuple[i].outInsts[0]->getType(),
                            instTuple[i].outInsts[0]->getName());
  }

  // Replace the add instruction with the result
  for (unsigned i = 0; i < instTuple.size(); ++i) {
    instTuple[i].outInsts[0]->replaceAllUsesWith(result[i]);
    instTuple[i].outInsts[0]->eraseFromParent();
  }
}

bool SIMDAdd::runOnBasicBlock(BasicBlock &BB) {
  // FIXME: check if LLVM 3.1 provides alternatives to skipFunction
  // if (skipFunction(F))
  Function *F = BB.getParent();
  if (F->getName().startswith("_ssdm_op") || F->getName() == SIMDOp)
    return false;

  // Get the SIMD function
  Module *module = F->getParent();
  Function *SIMDFunc = module->getFunction(SIMDOp);
  assert(SIMDFunc && "SIMD function not found");

  LLVMContext &context = F->getContext();

  AA = &getAnalysis<AAResultsWrapperPass>().getAAResults();

  bool modified = false;

  std::list<CandidateInst> candidateInsts;
  getSIMDableInstructions(BB, candidateInsts);

  candidateInsts.reverse();
  for (auto &candidateInstCurr : candidateInsts)
    anticipateDefs(candidateInstCurr.inInsts[0]);
  candidateInsts.reverse();
  for (auto &candidateInstCurr : candidateInsts)
    posticipateUses(candidateInstCurr.outInsts[0]);

  // Build tuples of 4 instructions that can be mapped to the
  // same SIMD DSP.
  // TODO: check if a size of 8 is a good choice
  while (!candidateInsts.empty()) {
    SmallVector<CandidateInst, 4> instTuple;
    Instruction *lastDef = nullptr;
    Instruction *firstUse = nullptr;

    DenseMap<Instruction *, int> instMap;
    getInstMap(&BB, instMap);
    for (auto CI = candidateInsts.begin(), CE = candidateInsts.end();
         CI != CE;) {
      CandidateInst candidateInstCurr = *CI;
      Instruction *lastDefCurr =
          getLastOperandDef(candidateInstCurr.inInsts[0]);
      Instruction *firstUseCurr =
          getFirstValueUse(candidateInstCurr.outInsts[0]);

      if ((!lastDefCurr) ||
          (lastDef && (instMap[lastDefCurr] < instMap[lastDef])))
        lastDefCurr = lastDef;

      if ((!firstUseCurr) ||
          (firstUse && (instMap[firstUse] < instMap[firstUseCurr])))
        firstUseCurr = firstUse;

      // If firstUseCurr is before lastDefCurr this pair of instructions is not
      // compatible with current tuple.
      if (firstUseCurr && lastDefCurr &&
          (instMap[firstUseCurr] < instMap[lastDefCurr])) {
        CI++;
        continue;
      }

      auto compatible = true;
      auto opInst = dyn_cast<Instruction>(candidateInstCurr.inInsts[0]);
      for (auto selected : instTuple) {
        if (dependsOn(opInst, selected.outInsts[0])) {
          compatible = false;
          break;
        }
      }
      if (!compatible) {
        CI++;
        continue;
      }

      instTuple.push_back(candidateInstCurr);

      // Update with tuple worst case
      firstUse = firstUseCurr;
      lastDef = lastDefCurr;

      // The current candidate was selected: it is not a candidate anymore.
      candidateInsts.erase(CI++);

      if (instTuple.size() == 4)
        break;
    }

    // TODO: maybe also skip tuples of size 2?
    if (instTuple.size() < 2)
      continue;

    replaceInstsWithSIMDCall(instTuple, firstUse, SIMDFunc, context);
    modified = true;
  }

  return modified;
}

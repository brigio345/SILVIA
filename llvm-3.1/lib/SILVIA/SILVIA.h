#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/BasicBlock.h"
#include "llvm/Function.h"
#include "llvm/Instructions.h"
#include "llvm/Module.h"
#include "llvm/Pass.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/IRBuilder.h"
#include "llvm/Support/raw_ostream.h"

#include <cassert>
#include <cmath>
#include <list>

using namespace llvm;

struct SILVIA : public BasicBlockPass {
  static char ID;
  SILVIA(char ID) : BasicBlockPass(ID) {}

  struct Candidate {
    SmallVector<Value *, 2> inVals;
    Instruction *outInst;
  };

  virtual bool runOnBasicBlock(BasicBlock &BB);

  void getAnalysisUsage(AnalysisUsage &AU) const {
    AU.addRequired<AliasAnalysis>();
  }

  static Value *getUnextendedValue(Value *V);
  static int getExtOpcode(Instruction *I);

  virtual std::list<SILVIA::Candidate> getSIMDableInstructions(BasicBlock &BB) {
    return std::list<SILVIA::Candidate>();
  }
  bool anticipateDefs(Instruction *inst, bool anticipateInst);
  bool posticipateUses(Instruction *inst, bool posticipateInst);
  Instruction *getFirstAliasingInst(Instruction *instToMove,
                                    Instruction *firstInst,
                                    Instruction *lastInst);
  Instruction *getLastAliasingInst(Instruction *instToMove,
                                   Instruction *firstInst,
                                   Instruction *lastInst);
  virtual bool
  isCandidateCompatibleWithTuple(SILVIA::Candidate &candidate,
                                 SmallVector<SILVIA::Candidate, 4> &tuple) {
    return false;
  }
  virtual bool isTupleFull(SmallVector<SILVIA::Candidate, 4> &tuple) {
    return true;
  }
  virtual Value *packTuple(SmallVector<SILVIA::Candidate, 4> instTuple,
                           Instruction *insertBefore, LLVMContext &context) {
    return nullptr;
  }

  AliasAnalysis *AA;
};

bool dependsOn(Instruction *inst0, Instruction *inst1) {
  if (inst0->getParent() != inst1->getParent())
    return false;

  if (inst0 == inst1)
    return true;

  for (unsigned i = 0; i < inst0->getNumOperands(); ++i) {
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

Instruction *SILVIA::getFirstAliasingInst(Instruction *instToMove,
                                          Instruction *firstInst,
                                          Instruction *lastInst) {
  auto loadToMove = dyn_cast<LoadInst>(instToMove);
  auto storeToMove = dyn_cast<StoreInst>(instToMove);

  if ((!loadToMove) && (!storeToMove))
    return nullptr;

  AliasAnalysis::Location locToMove =
      (storeToMove ? AA->getLocation(storeToMove)
                   : AA->getLocation(loadToMove));

  bool toCheck = false;
  for (auto &I : *(instToMove->getParent())) {
    // Skip the instructions before the interval involved in the movement.
    if ((!toCheck) && (&I != firstInst))
      continue;
    toCheck = true;

    // Skip the instructions after the interval involved in the movement.
    if (&I == lastInst)
      break;

    if (&I == instToMove)
      continue;

    if (dyn_cast<CallInst>(&I))
      return &I;

    if (auto store = dyn_cast<StoreInst>(&I)) {
      auto loc = AA->getLocation(store);

      if (AA->alias(locToMove, loc) != AliasAnalysis::AliasResult::NoAlias)
        return &I;
    }

    // If a load aliases with another load is not an issue. There is no need to
    // check.
    if (loadToMove)
      continue;

    if (auto load = dyn_cast<LoadInst>(&I)) {
      auto loc = AA->getLocation(load);

      if (AA->alias(locToMove, loc) != AliasAnalysis::AliasResult::NoAlias)
        return &I;
    }
  }

  return nullptr;
}

Instruction *SILVIA::getLastAliasingInst(Instruction *instToMove,
                                         Instruction *firstInst,
                                         Instruction *lastInst) {
  auto loadToMove = dyn_cast<LoadInst>(instToMove);
  auto storeToMove = dyn_cast<StoreInst>(instToMove);

  if ((!loadToMove) && (!storeToMove))
    return nullptr;

  AliasAnalysis::Location locToMove =
      (storeToMove ? AA->getLocation(storeToMove)
                   : AA->getLocation(loadToMove));

  bool toCheck = false;
  auto BB = instToMove->getParent();
  auto &instList = BB->getInstList();
  for (auto BI = instList.rbegin(), BE = instList.rend(); BI != BE; ++BI) {
    auto &I = *BI;
    // Skip the instructions after the interval involved in the movement.
    if ((!toCheck) && (&I != lastInst))
      continue;
    toCheck = true;

    // Skip the instructions before the interval involved in the movement.
    if (&I == firstInst)
      break;

    if (&I == instToMove)
      continue;

    if (dyn_cast<CallInst>(&I))
      return &I;

    if (auto store = dyn_cast<StoreInst>(&I)) {
      auto loc = AA->getLocation(store);

      if (AA->alias(locToMove, loc) != AliasAnalysis::AliasResult::NoAlias)
        return &I;
    }

    // If a load aliases with another load is not an issue. There is no need to
    // check.
    if (loadToMove)
      continue;

    if (auto load = dyn_cast<LoadInst>(&I)) {
      auto loc = AA->getLocation(load);

      if (AA->alias(locToMove, loc) != AliasAnalysis::AliasResult::NoAlias)
        return &I;
    }
  }

  return nullptr;
}

Value *SILVIA::getUnextendedValue(Value *V) {
  if (auto I = dyn_cast<Instruction>(V)) {
    if ((I->getOpcode() == Instruction::SExt) ||
        (I->getOpcode() == Instruction::ZExt))
      return SILVIA::getUnextendedValue(I->getOperand(0));
  }

  return V;
}

int SILVIA::getExtOpcode(Instruction *I) {
  for (auto UI = I->use_begin(), UE = I->use_end(); UI != UE; ++UI) {
    Instruction *user = dyn_cast<Instruction>(*UI);
    auto userOpcode = user->getOpcode();
    if ((userOpcode == Instruction::SExt) || (userOpcode == Instruction::ZExt))
      return userOpcode;
  }

  return -1;
}

bool SILVIA::anticipateDefs(Instruction *inst, bool anticipateInst = false) {
  // TODO: Anticipate calls if not crossing other calls or loads/stores.
  auto opcode = inst->getOpcode();
  if ((opcode == Instruction::PHI) || (opcode == Instruction::Call))
    return false;

  auto modified = false;
  BasicBlock *instBB = inst->getParent();
  for (unsigned i = 0; i < inst->getNumOperands(); ++i) {
    Value *op = inst->getOperand(i);
    auto opInst = dyn_cast<Instruction>(op);
    if (!opInst)
      continue;
    if (opInst->getParent() == instBB)
      modified = anticipateDefs(opInst, true);
  }

  if (!anticipateInst)
    return modified;

  Instruction *insertionPoint = instBB->getFirstNonPHI();
  auto lastDef = getLastOperandDef(inst);
  if (lastDef && (lastDef->getOpcode() != Instruction::PHI))
    insertionPoint = lastDef->getNextNode();

  auto aliasingInst = getLastAliasingInst(inst, insertionPoint, inst);
  if (aliasingInst)
    insertionPoint = aliasingInst->getNextNode();

  inst->moveBefore(insertionPoint);

  return true;
}

bool SILVIA::posticipateUses(Instruction *inst, bool posticipateInst = false) {
  // TODO: Posticipate calls if not crossing other calls or loads/stores.
  auto opcode = inst->getOpcode();
  if ((opcode == Instruction::PHI) || (opcode == Instruction::Call))
    return false;

  auto modified = false;
  BasicBlock *instBB = inst->getParent();
  for (auto UI = inst->use_begin(), UE = inst->use_end(); UI != UE; ++UI) {
    Value *user = *UI;
    auto userInst = dyn_cast<Instruction>(user);
    if (!userInst)
      continue;
    if (userInst->getParent() == instBB)
      modified = posticipateUses(userInst, true);
  }

  if (!posticipateInst)
    return modified;

  Instruction *insertionPoint = getFirstValueUse(inst);
  if (!insertionPoint)
    insertionPoint = instBB->getTerminator();

  auto aliasingInst = getFirstAliasingInst(inst, insertionPoint, inst);
  if (aliasingInst)
    insertionPoint = aliasingInst;

  inst->moveBefore(insertionPoint);

  return true;
}

bool SILVIA::runOnBasicBlock(BasicBlock &BB) {
  Function *F = BB.getParent();
  DEBUG(dbgs() << "SILVIA::runOnBasicBlock: called on " << F->getName() << " @ "
               << BB.getName() << ".\n");
  if (F->getName().startswith("_ssdm_op") || F->getName().startswith("_simd"))
    return false;

  LLVMContext &context = F->getContext();

  AA = &getAnalysis<AliasAnalysis>();

  std::list<SILVIA::Candidate> candidateInsts = getSIMDableInstructions(BB);
  DEBUG(if (candidateInsts.size() > 0) dbgs()
        << "SILVIA::getSIMDableInstructions: found " << candidateInsts.size()
        << " candidates.\n");

  if (candidateInsts.size() < 2)
    return false;

  bool modified = false;

  candidateInsts.reverse();
  for (auto &candidateInstCurr : candidateInsts)
    modified |= anticipateDefs(candidateInstCurr.outInst);
  candidateInsts.reverse();
  for (auto &candidateInstCurr : candidateInsts)
    modified |= posticipateUses(candidateInstCurr.outInst);

  // Build tuples of SIMDFactor instructions that can be mapped to the
  // same SIMD DSP.
  while (!candidateInsts.empty()) {
    SmallVector<SILVIA::Candidate, 4> instTuple;
    Instruction *lastDef = nullptr;
    Instruction *firstUse = nullptr;

    DenseMap<Instruction *, int> instMap;
    getInstMap(&BB, instMap);
    for (auto CI = candidateInsts.begin(), CE = candidateInsts.end();
         CI != CE;) {
      SILVIA::Candidate candidateInstCurr = *CI;
      Instruction *lastDefCurr = getLastOperandDef(candidateInstCurr.outInst);
      Instruction *firstUseCurr = getFirstValueUse(candidateInstCurr.outInst);

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
      auto opInst = dyn_cast<Instruction>(candidateInstCurr.outInst);
      for (auto selected : instTuple) {
        if (dependsOn(opInst, selected.outInst)) {
          compatible = false;
          break;
        }
      }
      if (!compatible) {
        CI++;
        continue;
      }

      if (!isCandidateCompatibleWithTuple(candidateInstCurr, instTuple)) {
        CI++;
        continue;
      }

      instTuple.push_back(candidateInstCurr);

      // Update with tuple worst case
      firstUse = firstUseCurr;
      lastDef = lastDefCurr;

      // The current candidate was selected: it is not a candidate anymore.
      candidateInsts.erase(CI++);

      if (isTupleFull(instTuple))
        break;
    }

    if (instTuple.size() < 2)
      continue;

    DEBUG(dbgs() << "SILVIA::runOnBasicBlock: found a tuple of "
                 << instTuple.size() << " elements.\n");
    auto insertBefore = (firstUse ? firstUse : BB.getTerminator());
    auto pack = packTuple(instTuple, insertBefore, context);

    if (!pack)
      continue;

    DEBUG(dbgs() << "SILVIA::runOnBasicBlock: packed a tuple of "
                 << instTuple.size() << " elements.\n");

    IRBuilder<> builder(insertBefore);
    for (unsigned i = 0; i < instTuple.size(); ++i) {
      std::string origName = instTuple[i].outInst->getName();
      auto packedInst = builder.CreateExtractValue(pack, i);
      instTuple[i].outInst->replaceAllUsesWith(packedInst);
      instTuple[i].outInst->eraseFromParent();
      packedInst->setName(origName);
    }

    modified = true;
  }

  return modified;
}

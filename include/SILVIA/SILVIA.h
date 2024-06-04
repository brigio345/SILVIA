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

  bool doInitialization(Module &M) override {
    DEBUG(packedTuples = 0);
    DEBUG(packedCandidates = 0);
    return false;
  }

  bool doFinalization(Module &M) override {
    printReport();
    return false;
  }

  static Value *getUnextendedValue(Value *V);
  static int getExtOpcode(Instruction *I);

  virtual std::list<SILVIA::Candidate> getCandidates(BasicBlock &BB) {
    return std::list<SILVIA::Candidate>();
  }
  bool moveUsesALAP(Instruction *inst, Instruction *barrier,
                    bool posticipateInst);
  Instruction *getFirstAliasingInst(Instruction *instToMove,
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
  virtual void printReport() {
    DEBUG(dbgs() << "SILVIA::printReport: packed " << packedTuples
                 << " tuples (" << packedCandidates << " candidates).\n");
  }

  AliasAnalysis *AA;
  unsigned long packedTuples;
  unsigned long packedCandidates;
};

void getInstMap(const BasicBlock *const BB,
                DenseMap<const Instruction *, int> &instMap) {
  instMap.clear();
  for (const auto &inst : *BB)
    instMap[&inst] = instMap.size();
}

Instruction *getLastOperandDef(const Instruction *const inst) {
  auto instBB = inst->getParent();

  DenseMap<const Instruction *, int> instMap;
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

  DenseMap<const Instruction *, int> instMap;
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

bool mayHaveSideEffects(Function *F) {
  auto FName = F->getName();
  return (!(FName.startswith("llvm.dbg.value") ||
            FName.startswith("_ssdm_op_SparseMux") ||
            FName.startswith("_ssdm_op_BitSelect") ||
            FName.startswith("_ssdm_op_BitConcatenate") ||
            FName.startswith("_ssdm_op_PartSelect")));
}

Instruction *SILVIA::getFirstAliasingInst(Instruction *instToMove,
                                          Instruction *firstInst,
                                          Instruction *lastInst) {
  auto loadToMove = dyn_cast<LoadInst>(instToMove);
  auto storeToMove = dyn_cast<StoreInst>(instToMove);
  auto callToMove = dyn_cast<CallInst>(instToMove);

  if (callToMove && (!mayHaveSideEffects(callToMove->getCalledFunction())))
    return nullptr;

  if ((!loadToMove) && (!storeToMove) && (!callToMove))
    return nullptr;

  AliasAnalysis::Location locToMove;
  if (storeToMove)
    locToMove = AA->getLocation(storeToMove);
  else if (loadToMove)
    locToMove = AA->getLocation(loadToMove);

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

    if (auto call = dyn_cast<CallInst>(&I)) {
      if (mayHaveSideEffects(call->getCalledFunction()))
        return &I;
    }

    if (auto store = dyn_cast<StoreInst>(&I)) {
      if (callToMove)
        return &I;

      auto loc = AA->getLocation(store);

      if (AA->alias(locToMove, loc) != AliasAnalysis::AliasResult::NoAlias)
        return &I;
    }

    // If a load aliases with another load is not an issue. There is no need to
    // check.
    if (loadToMove)
      continue;

    if (auto load = dyn_cast<LoadInst>(&I)) {
      if (callToMove)
        return &I;

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
  int opcode = -1;
  for (unsigned i = 0; i < I->getNumOperands(); ++i) {
    const auto op = dyn_cast<Instruction>(I->getOperand(i));
    auto opOpcode = op->getOpcode();
    if (opOpcode == Instruction::SExt)
      return opOpcode;
    if (opOpcode == Instruction::ZExt)
      opcode = Instruction::ZExt;
  }

  return opcode;
}

bool SILVIA::moveUsesALAP(Instruction *inst, Instruction *barrier = nullptr,
                          bool posticipateInst = false) {
  if (inst == barrier)
    return false;

  auto opcode = inst->getOpcode();
  if (opcode == Instruction::PHI)
    return false;

  if (barrier) {
    DenseMap<const Instruction *, int> instMap;
    getInstMap(inst->getParent(), instMap);
    if (instMap[inst] >= instMap[barrier])
      return false;
  }

  auto modified = false;
  BasicBlock *instBB = inst->getParent();
  for (auto UI = inst->use_begin(), UE = inst->use_end(); UI != UE; ++UI) {
    Value *user = *UI;
    auto userInst = dyn_cast<Instruction>(user);
    if (!userInst)
      continue;
    if (userInst->getParent() == instBB)
      modified = moveUsesALAP(userInst, barrier, true);
  }

  if (!posticipateInst)
    return modified;

  Instruction *insertionPoint = getFirstValueUse(inst);
  if (!insertionPoint)
    insertionPoint = instBB->getTerminator();

  // Move the aliasing instructions ALAP and recompute the insertion point.
  if (auto aliasingInst = getFirstAliasingInst(inst, inst, insertionPoint)) {
    moveUsesALAP(aliasingInst, barrier, true);

    insertionPoint = getFirstValueUse(inst);
    if (!insertionPoint)
      insertionPoint = instBB->getTerminator();

    aliasingInst = getFirstAliasingInst(inst, inst, insertionPoint);
    if (aliasingInst)
      insertionPoint = aliasingInst;
  }

  if (barrier) {
    DenseMap<const Instruction *, int> instMap;
    getInstMap(inst->getParent(), instMap);
    if (instMap[insertionPoint] > instMap[barrier])
      insertionPoint = barrier;
  }

  inst->moveBefore(insertionPoint);

  return true;
}

void getDefsUsesInterval(SmallVector<SILVIA::Candidate, 4> &tuple,
                         Instruction *&lastDef, Instruction *&firstUse) {
  lastDef = nullptr;
  firstUse = nullptr;

  if (tuple.size() < 1)
    return;

  DenseMap<const Instruction *, int> instMap;
  getInstMap(tuple[0].outInst->getParent(), instMap);
  for (const auto &candidate : tuple) {
    Instruction *lastDefCand = getLastOperandDef(candidate.outInst);
    Instruction *firstUseCand = getFirstValueUse(candidate.outInst);

    if ((!lastDef) ||
        (lastDefCand && (instMap[lastDef] < instMap[lastDefCand])))
      lastDef = lastDefCand;

    if ((!firstUse) ||
        (firstUseCand && (instMap[firstUseCand] < instMap[firstUse])))
      firstUse = firstUseCand;
  }
}

bool SILVIA::runOnBasicBlock(BasicBlock &BB) {
  Function *F = BB.getParent();
  DEBUG(dbgs() << "SILVIA::runOnBasicBlock: called on " << F->getName() << " @ "
               << BB.getName() << ".\n");
  if (F->getName().startswith("_ssdm_op") || F->getName().startswith("_simd"))
    return false;

  LLVMContext &context = F->getContext();

  AA = &getAnalysis<AliasAnalysis>();

  std::list<SILVIA::Candidate> candidateInsts = getCandidates(BB);
  DEBUG(if (candidateInsts.size() > 0) dbgs() << "SILVIA::getCandidates: found "
                                              << candidateInsts.size()
                                              << " candidates.\n");

  if (candidateInsts.size() < 2)
    return false;

  bool modified = false;

  // Build tuples of SIMDFactor instructions that can be mapped to the
  // same SIMD DSP.
  while (candidateInsts.size() > 1) {
    SmallVector<SILVIA::Candidate, 4> instTuple;

    instTuple.push_back(candidateInsts.front());
    candidateInsts.pop_front();

    modified |= moveUsesALAP(instTuple[0].outInst);

    Instruction *lastDef = nullptr;
    Instruction *firstUse = nullptr;
    getDefsUsesInterval(instTuple, lastDef, firstUse);

    for (auto CI = candidateInsts.begin(), CE = candidateInsts.end();
         CI != CE;) {
      SILVIA::Candidate candidateInstCurr = *CI;

      if (!isCandidateCompatibleWithTuple(candidateInstCurr, instTuple)) {
        CI++;
        continue;
      }

      modified |= moveUsesALAP(candidateInstCurr.outInst, firstUse);

      getDefsUsesInterval(instTuple, lastDef, firstUse);
      Instruction *lastDefCurr = getLastOperandDef(candidateInstCurr.outInst);
      Instruction *firstUseCurr = getFirstValueUse(candidateInstCurr.outInst);

      DenseMap<const Instruction *, int> instMap;
      getInstMap(&BB, instMap);

      // Check if the current candidate uses the tuple or if the tuple uses the
      // current candidate.
      auto compatible = true;
      for (const auto &selected : instTuple) {
        for (const auto &inVal : candidateInstCurr.inVals) {
          if (inVal == selected.outInst) {
            compatible = false;
            break;
          }
        }
        if (!compatible)
          break;

        for (const auto &inVal : selected.inVals) {
          if (inVal == candidateInstCurr.outInst) {
            compatible = false;
            break;
          }
        }
        if (!compatible)
          break;
      }
      if (!compatible) {
        CI++;
        continue;
      }

      if ((!lastDefCurr) ||
          (lastDef && (instMap[lastDefCurr] < instMap[lastDef])))
        lastDefCurr = lastDef;

      if ((!firstUseCurr) ||
          (firstUse && (instMap[firstUse] < instMap[firstUseCurr])))
        firstUseCurr = firstUse;

      // If firstUseCurr is before lastDefCurr this pair of instructions is not
      // compatible with current tuple.
      if (firstUseCurr && lastDefCurr &&
          (instMap[firstUseCurr] <= instMap[lastDefCurr])) {
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

    DEBUG(packedTuples++);
    DEBUG(packedCandidates += instTuple.size());
    DEBUG(dbgs() << "SILVIA::runOnBasicBlock: packed a tuple of "
                 << instTuple.size() << " elements.\n");

    IRBuilder<> builder(insertBefore);
    for (unsigned i = 0; i < instTuple.size(); ++i) {
      std::string origName = instTuple[i].outInst->getName();
      Instruction *packedInst =
          cast<Instruction>(builder.CreateExtractValue(pack, i));
      for (auto &candidate : candidateInsts) {
        for (auto &inVal : candidate.inVals) {
          if (inVal == instTuple[i].outInst)
            inVal = packedInst;
        }
      }
      if (insertBefore == instTuple[i].outInst) {
        insertBefore = packedInst;
        builder.SetInsertPoint(insertBefore);
      }
      instTuple[i].outInst->replaceAllUsesWith(packedInst);
      instTuple[i].outInst->eraseFromParent();
      packedInst->setName(origName);
    }

    modified = true;
  }

  return modified;
}

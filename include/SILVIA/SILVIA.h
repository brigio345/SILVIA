#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/DenseSet.h"
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
#include <unordered_set>

using namespace llvm;

struct SILVIA : public BasicBlockPass {
  static char ID;
  SILVIA(char ID, bool SINGLE_OCCURRENCE)
      : BasicBlockPass(ID), SINGLE_OCCURRENCE(SINGLE_OCCURRENCE) {}

  struct Candidate {
    SmallVector<Value *, 2> inVals;
    Instruction *outInst;

    bool operator==(const Candidate &other) const {
      return ((inVals == other.inVals) && (outInst == other.outInst));
    }

    struct Hash {
      size_t operator()(const Candidate &candidate) const {
        // TODO: Define a proper hash function.
        return reinterpret_cast<size_t>(candidate.outInst);
      }
    };
  };

  virtual bool runOnBasicBlock(BasicBlock &BB);

  void getAnalysisUsage(AnalysisUsage &AU) const {
    AU.addRequired<AliasAnalysis>();
  }

  bool doInitialization(Module &M) override {
    DEBUG(numPackedTuples = 0);
    DEBUG(numPackedCandidates = 0);
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
  bool moveUsesALAP(Instruction *inst);
  bool moveUsesALAP(Instruction *inst, bool postponeInst,
                    DenseSet<Instruction *> &postponed);
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
    DEBUG(dbgs() << "SILVIA::printReport: packed " << numPackedTuples
                 << " tuples (" << numPackedCandidates << " candidates).\n");
  }

  bool
  getAllTuples(std::list<SILVIA::Candidate>::iterator &candidatesIt,
               std::list<SILVIA::Candidate>::iterator &candidatesEnd,
               SmallVector<SILVIA::Candidate, 4> &tuple,
               std::unordered_set<SILVIA::Candidate, SILVIA::Candidate::Hash>
                   &tupledCandidates,
               std::list<SmallVector<SILVIA::Candidate, 4>> &tuples);
  std::list<SmallVector<SILVIA::Candidate, 4>>
  getAllTuples(std::list<SILVIA::Candidate> &candidates);

  AliasAnalysis *AA;
  unsigned long numPackedTuples;
  unsigned long numPackedCandidates;
  const bool SINGLE_OCCURRENCE;
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
  return (!(FName.startswith("_silvia") ||
            FName.startswith("llvm.dbg.value") ||
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

bool SILVIA::moveUsesALAP(Instruction *inst) {
  DenseSet<Instruction *> postponed;
  return moveUsesALAP(inst, false, postponed);
}

bool SILVIA::moveUsesALAP(Instruction *inst, bool postponeInst,
                          DenseSet<Instruction *> &postponed) {
  if (postponed.count(inst))
    return false;

  auto opcode = inst->getOpcode();
  if (opcode == Instruction::PHI)
    return false;

  auto modified = false;
  BasicBlock *instBB = inst->getParent();
  for (auto UI = inst->use_begin(), UE = inst->use_end(); UI != UE; ++UI) {
    Value *user = *UI;
    auto userInst = dyn_cast<Instruction>(user);
    if (!userInst)
      continue;
    if (userInst->getParent() == instBB)
      modified = moveUsesALAP(userInst, true, postponed);
  }

  if (!postponeInst)
    return modified;

  Instruction *insertionPoint = getFirstValueUse(inst);
  if (!insertionPoint)
    insertionPoint = instBB->getTerminator();

  // Move the aliasing instructions ALAP and recompute the insertion point.
  if (auto aliasingInst = getFirstAliasingInst(inst, inst, insertionPoint)) {
    moveUsesALAP(aliasingInst, true, postponed);

    insertionPoint = getFirstValueUse(inst);
    if (!insertionPoint)
      insertionPoint = instBB->getTerminator();

    aliasingInst = getFirstAliasingInst(inst, inst, insertionPoint);
    if (aliasingInst)
      insertionPoint = aliasingInst;
  }

  inst->moveBefore(insertionPoint);

  postponed.insert(inst);

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

bool SILVIA::getAllTuples(
    std::list<SILVIA::Candidate>::iterator &candidatesIt,
    std::list<SILVIA::Candidate>::iterator &candidatesEnd,
    SmallVector<SILVIA::Candidate, 4> &tuple,
    std::unordered_set<SILVIA::Candidate, SILVIA::Candidate::Hash>
        &tupledCandidates,
    std::list<SmallVector<SILVIA::Candidate, 4>> &tuples) {
  if (isTupleFull(tuple)) {
    tuples.push_back(tuple);
    if (SINGLE_OCCURRENCE) {
      for (const auto &candidate : tuple)
        tupledCandidates.insert(candidate);
    }
    return true;
  }

  auto tupled = false;
  for (; candidatesIt != candidatesEnd; ++candidatesIt) {
    SILVIA::Candidate candidate = *candidatesIt;

    if (SINGLE_OCCURRENCE && tupledCandidates.count(candidate))
      continue;

    auto canPack = true;
    // Skip candidates using or used by other candidates within the tuple.
    for (const auto &selected : tuple) {
      for (const auto &inVal : candidate.inVals) {
        if (inVal == selected.outInst) {
          canPack = false;
          break;
        }
      }
      if (!canPack)
        break;

      for (const auto &inVal : selected.inVals) {
        if (inVal == candidate.outInst) {
          canPack = false;
          break;
        }
      }
      if (!canPack)
        break;
    }
    if (!canPack)
      continue;

    if (!isCandidateCompatibleWithTuple(candidate, tuple))
      continue;

    tuple.push_back(candidate);

    Instruction *lastDef = nullptr;
    Instruction *firstUse = nullptr;
    getDefsUsesInterval(tuple, lastDef, firstUse);

    DenseMap<const Instruction *, int> instMap;
    getInstMap(candidate.outInst->getParent(), instMap);

    // If firstUse is before lastDef this tuple is not valid.
    if (firstUse && lastDef && (instMap[firstUse] <= instMap[lastDef])) {
      tuple.pop_back();
      continue;
    }

    auto candidatesNextIt = std::next(candidatesIt);
    const auto tupledRecur = getAllTuples(candidatesNextIt, candidatesEnd,
                                          tuple, tupledCandidates, tuples);
    tupled |= tupledRecur;
    tuple.pop_back();

    if (SINGLE_OCCURRENCE && tupledRecur) {
      if (tuple.size() > 0)
        return true;
      continue;
    }
  }

  if ((!tupled) && (tuple.size() > 1)) {
    tuples.push_back(tuple);
    if (SINGLE_OCCURRENCE) {
      for (const auto &candidate : tuple)
        tupledCandidates.insert(candidate);
    }

    return true;
  }

  return tupled;
}

// Return all the tuples of candidates that can be packed together.
std::list<SmallVector<SILVIA::Candidate, 4>>
SILVIA::getAllTuples(std::list<SILVIA::Candidate> &candidates) {
  auto candidatesBegin = candidates.begin();
  auto candidatesEnd = candidates.end();
  SmallVector<SILVIA::Candidate, 4> tuple;
  std::unordered_set<SILVIA::Candidate, SILVIA::Candidate::Hash>
      tupledCandidates;
  std::list<SmallVector<SILVIA::Candidate, 4>> tuples;
  getAllTuples(candidatesBegin, candidatesEnd, tuple, tupledCandidates, tuples);

  return tuples;
}

bool SILVIA::runOnBasicBlock(BasicBlock &BB) {
  Function *F = BB.getParent();
  DEBUG(dbgs() << "SILVIA::runOnBasicBlock: called on " << F->getName() << " @ "
               << BB.getName() << ".\n");
  if (F->getName().startswith("_ssdm_op") || F->getName().startswith("_silvia"))
    return false;

  LLVMContext &context = F->getContext();

  AA = &getAnalysis<AliasAnalysis>();

  std::list<SILVIA::Candidate> candidateInsts = getCandidates(BB);
  DEBUG(if (candidateInsts.size() > 0) dbgs() << "SILVIA::getCandidates: found "
                                              << candidateInsts.size()
                                              << " candidates.\n");

  // Sorting the input values of each candidate empirically proved to result in
  // a larger number of packs.
  DenseMap<const Instruction *, int> instMap;
  getInstMap(&BB, instMap);
  for (auto &candidate : candidateInsts) {
    std::sort(candidate.inVals.begin(), candidate.inVals.end(),
              [&](Value *a, Value *b) {
                return instMap[cast<Instruction>(a)] <
                       instMap[cast<Instruction>(b)];
              });
  }

  if (candidateInsts.size() < 2)
    return false;

  for (const auto &candidate : candidateInsts)
    moveUsesALAP(candidate.outInst);

  auto tuples = getAllTuples(candidateInsts);

  // Prioritize the tuples composed of larger amounts of candidates.
  tuples.sort([&](SmallVector<SILVIA::Candidate, 4> &a,
                  SmallVector<SILVIA::Candidate, 4> &b) {
    if (a.size() > b.size())
      return true;
    if (a.size() < b.size())
      return false;
    return (instMap[a[0].outInst] < instMap[b[0].outInst]);
  });

  bool modified = false;

  const auto numPackedTuplesBefore = numPackedTuples;
  const auto numPackedCandidatesBefore = numPackedCandidates;

  std::unordered_set<SILVIA::Candidate, SILVIA::Candidate::Hash>
      packedCandidates;

  // Pack the tuples not containing already packed candidates.
  for (auto &tuple : tuples) {
    auto canPack = true;
    // TODO: Accept tuples containing non-packed candidates (?)
    for (const auto &candidate : tuple) {
      if (packedCandidates.count(candidate)) {
        canPack = false;
        break;
      }
    }
    if (!canPack)
      continue;

    DEBUG(dbgs() << "SILVIA::runOnBasicBlock: found a tuple of " << tuple.size()
                 << " elements.\n");

    Instruction *lastDef = nullptr;
    Instruction *firstUse = nullptr;
    getDefsUsesInterval(tuple, lastDef, firstUse);
    auto insertBefore = (firstUse ? firstUse : BB.getTerminator());
    auto pack = packTuple(tuple, insertBefore, context);

    if (!pack)
      continue;

    for (const auto &candidate : tuple)
      packedCandidates.insert(candidate);

    DEBUG(numPackedTuples++);
    DEBUG(numPackedCandidates += tuple.size());
    DEBUG(dbgs() << "SILVIA::runOnBasicBlock: packed a tuple of "
                 << tuple.size() << " elements.\n");

    IRBuilder<> builder(insertBefore);
    for (unsigned i = 0; i < tuple.size(); ++i) {
      std::string origName = tuple[i].outInst->getName();
      Instruction *packedInst =
          cast<Instruction>(builder.CreateExtractValue(pack, i));
      for (auto &candidate : candidateInsts) {
        for (auto &inVal : candidate.inVals) {
          if (inVal == tuple[i].outInst)
            inVal = packedInst;
        }
      }
      if (insertBefore == tuple[i].outInst) {
        insertBefore = packedInst;
        builder.SetInsertPoint(insertBefore);
      }
      tuple[i].outInst->replaceAllUsesWith(packedInst);
      tuple[i].outInst->eraseFromParent();
      packedInst->setName(origName);
    }

    modified = true;
  }

  DEBUG(if (numPackedTuples > numPackedTuplesBefore) {
    dbgs() << "SILVIA::runOnBasicBlock(" << BB.getName() << " @ "
           << BB.getParent()->getName() << "): packed "
           << (numPackedTuples - numPackedTuplesBefore) << " tuples ("
           << (numPackedCandidates - numPackedCandidatesBefore)
           << " candidates).\n";
  });

  return modified;
}

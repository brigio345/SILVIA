#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/BasicBlock.h"
#include "llvm/Function.h"
#include "llvm/Instructions.h"
#include "llvm/Module.h"
#include "llvm/Pass.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/IRBuilder.h"

#include <cassert>
#include <list>

using namespace llvm;

struct SIMDAdd : public BasicBlockPass {
  static char ID;
  SIMDAdd() : BasicBlockPass(ID) {}

  bool runOnBasicBlock(BasicBlock &BB) override;

  virtual void getAnalysisUsage(AnalysisUsage &AU) const {
    AU.addRequired<AliasAnalysis>();
  }

  bool anticipateDefs(Instruction *inst, bool anticipateInst);
  bool posticipateUses(Instruction *inst, bool posticipateInst);
  Instruction *getFirstAliasingInst(Instruction *instToMove,
                                    Instruction *firstInst,
                                    Instruction *lastInst);
  Instruction *getLastAliasingInst(Instruction *instToMove,
                                   Instruction *firstInst,
                                   Instruction *lastInst);

  AliasAnalysis *AA;
};

struct CandidateInst {
  SmallVector<Instruction *, 2> inInsts;
  Instruction *outInst;
};

char SIMDAdd::ID = 0;
static RegisterPass<SIMDAdd> X("simd-add", "Map add instructions to SIMD DSPs",
                               false /* Only looks at CFG */,
                               true /* Transformation Pass */);

static cl::opt<std::string>
    SIMDOp("simd-add-op", cl::init("add"), cl::Hidden,
           cl::desc("The operation to map to SIMD DSPs. "
                    "Possible values are: add."));

static cl::opt<unsigned int>
    SIMDFactor("simd-add-factor", cl::init(4), cl::Hidden,
               cl::desc("The amount of operations to map to SIMD DSPs."));

static cl::opt<unsigned int> SIMDDSPWidth("simd-add-dsp-width", cl::init(48),
                                          cl::Hidden,
                                          cl::desc("The DSP width in bits."));

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

Instruction *SIMDAdd::getFirstAliasingInst(Instruction *instToMove,
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

Instruction *SIMDAdd::getLastAliasingInst(Instruction *instToMove,
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

bool SIMDAdd::anticipateDefs(Instruction *inst, bool anticipateInst = false) {
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

bool SIMDAdd::posticipateUses(Instruction *inst, bool posticipateInst = false) {
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

// Collect all the add instructions.
std::list<CandidateInst>
getSIMDableInstructions(BasicBlock &BB, const unsigned int SIMDFactor,
                        const unsigned int SIMDDSPWidth) {
  std::list<CandidateInst> candidateInsts;

  const auto addMaxWidth = (SIMDDSPWidth / SIMDFactor);

  for (auto &I : BB) {
    if (I.getOpcode() != Instruction::Add)
      continue;
    if (isa<Constant>(I.getOperand(0)) || isa<Constant>(I.getOperand(1)))
      continue;
    if (I.getType()->getScalarSizeInBits() <= addMaxWidth) {
      CandidateInst candidate;
      candidate.inInsts.push_back(&I);
      candidate.outInst = &I;
      candidateInsts.push_back(candidate);
    }
  }

  return candidateInsts;
}

void replaceInstsWithSIMDCall(SmallVector<CandidateInst, 4> instTuple,
                              Instruction *insertBefore, Function *SIMDFunc,
                              const unsigned int SIMDFactor,
                              const unsigned int SIMDDSPWidth,
                              LLVMContext &context) {
  IRBuilder<> builder(insertBefore);

  const auto dataBitWidth = (SIMDDSPWidth / SIMDFactor);

  SmallVector<Value *, 8> args(
      SIMDFunc->getArgumentList().size(),
      ConstantInt::get(IntegerType::get(context, dataBitWidth), 0));
  std::string retName = "";
  for (unsigned i = 0; i < instTuple.size(); ++i) {
    retName = retName + ((retName == "") ? "" : "_") +
              instTuple[i].outInst->getName().str();
    for (unsigned j = 0; j < instTuple[i].inInsts[0]->getNumOperands(); ++j) {
      auto operand = instTuple[i].inInsts[0]->getOperand(j);

      auto arg = ((operand->getType()->getScalarSizeInBits() < dataBitWidth)
                      ? builder.CreateZExt(
                            operand, IntegerType::get(context, dataBitWidth),
                            operand->getName() + "_zext")
                      : operand);
      args[i * instTuple[i].inInsts[0]->getNumOperands() + j] = arg;
    }
  }

  Value *sumAggr = builder.CreateCall(SIMDFunc, args, retName);

  Value *result[4];
  for (int i = 0; i < instTuple.size(); ++i) {
    result[i] = builder.CreateExtractValue(
        sumAggr, i, instTuple[i].outInst->getName() + "_zext");
    if (instTuple[i].outInst->getType()->getScalarSizeInBits() < dataBitWidth)
      result[i] =
          builder.CreateTrunc(result[i], instTuple[i].outInst->getType());
  }

  // Replace the add instruction with the result
  for (unsigned i = 0; i < instTuple.size(); ++i) {
    auto resName = instTuple[i].outInst->getName();

    instTuple[i].outInst->replaceAllUsesWith(result[i]);
    instTuple[i].outInst->eraseFromParent();

    result[i]->setName(resName);
  }
}

bool SIMDAdd::runOnBasicBlock(BasicBlock &BB) {
  assert((SIMDOp == "add") && "Unexpected value for simd-add-op option.");
  assert(((SIMDFactor == 2) || (SIMDFactor == 4)) &&
         "Unexpected value for simd-add-factor option.");
  assert((SIMDDSPWidth == 48) &&
         "Unexpected value for simd-add-dsp-width option.");
  // FIXME: check if LLVM 3.1 provides alternatives to skipFunction
  // if (skipFunction(F))
  Function *F = BB.getParent();
  if (F->getName().startswith("_ssdm_op") || F->getName().startswith("_simd"))
    return false;

  // Get the SIMD function
  Module *module = F->getParent();
  Function *SIMDFunc =
      module->getFunction("_simd_" + SIMDOp + "_" + std::to_string(SIMDFactor));
  assert(SIMDFunc && "SIMD function not found");

  LLVMContext &context = F->getContext();

  AA = &getAnalysis<AliasAnalysis>();

  std::list<CandidateInst> candidateInsts =
      getSIMDableInstructions(BB, SIMDFactor, SIMDDSPWidth);

  if (candidateInsts.size() < 2)
    return false;

  bool modified = false;

  candidateInsts.reverse();
  for (auto &candidateInstCurr : candidateInsts)
    modified |= anticipateDefs(candidateInstCurr.inInsts[0]);
  candidateInsts.reverse();
  for (auto &candidateInstCurr : candidateInsts)
    modified |= posticipateUses(candidateInstCurr.outInst);

  // Build tuples of SIMDFactor instructions that can be mapped to the
  // same SIMD DSP.
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
      auto opInst = dyn_cast<Instruction>(candidateInstCurr.inInsts[0]);
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

      instTuple.push_back(candidateInstCurr);

      // Update with tuple worst case
      firstUse = firstUseCurr;
      lastDef = lastDefCurr;

      // The current candidate was selected: it is not a candidate anymore.
      candidateInsts.erase(CI++);

      if (instTuple.size() == SIMDFactor)
        break;
    }

    // TODO: maybe also skip tuples of size 2?
    if (instTuple.size() < 2)
      continue;

    replaceInstsWithSIMDCall(instTuple, firstUse, SIMDFunc, SIMDFactor,
                             SIMDDSPWidth, context);
    modified = true;
  }

  return modified;
}

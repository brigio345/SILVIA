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

struct DotProdTree {
  CandidateInst candidate;
  SmallVector<Instruction *, 8> addInternal;
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
std::list<CandidateInst> getSIMDableAdds(BasicBlock &BB,
                                         const unsigned int SIMDFactor,
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

bool getDotProdTree(Instruction *addRoot, DotProdTree &tree) {
  if (addRoot->getOpcode() != Instruction::Add)
    return false;

  for (unsigned i = 0; i < addRoot->getNumOperands(); ++i) {
    auto op = dyn_cast<Instruction>(addRoot->getOperand(i));
    if (!op)
      return false;

    if ((op->getOpcode() == Instruction::SExt) ||
        (op->getOpcode() == Instruction::ZExt))
      op = dyn_cast<Instruction>(op->getOperand(0));
    if (!op)
      return false;

    // The tree cannot absorb an op with multiple uses, since its value is
    // needed elsewhere too.
    // TODO: Accept multiple uses too. We need to split the chain in that point.
    if (!op->hasOneUse())
      return false;

    // TODO: One of the leafs can be an add (to be connected to PCIN).
    switch (op->getOpcode()) {
    case Instruction::Mul:
      tree.candidate.inInsts.push_back(op);
      break;
    case Instruction::Add:
      if (!getDotProdTree(op, tree))
        return false;
      tree.addInternal.push_back(op);
      break;
    default:
      return false;
    }
  }

  tree.candidate.outInst = addRoot;
  return true;
}

Value *getUnextendedValue(Value *V) {
  if (auto I = dyn_cast<Instruction>(V)) {
    if ((I->getOpcode() == Instruction::SExt) ||
        (I->getOpcode() == Instruction::ZExt))
      return getUnextendedValue(I->getOperand(0));
  }

  return V;
}

std::list<CandidateInst> getSIMDableMuladds(BasicBlock &BB) {
  SmallVector<DotProdTree, 8> trees;
  // Iterate in reverse order to avoid collecting subset trees.
  for (auto II = BB.end(), IB = BB.begin(); II != IB; --II) {
    Instruction *I = II;
    if (I->getOpcode() == Instruction::Add) {
      bool subset = false;
      for (auto tree : trees) {
        for (auto &add : tree.addInternal) {
          if (add == I) {
            subset = true;
            break;
          }
        }
        if (subset)
          break;
      }

      if (subset)
        continue;

      DotProdTree tree;
      if (getDotProdTree(I, tree)) {
        auto valid = false;
        for (auto inInst : tree.candidate.inInsts) {

          valid = ((getUnextendedValue(inInst->getOperand(0))
                        ->getType()
                        ->getScalarSizeInBits() <= 8) &&
                   ((getUnextendedValue(inInst->getOperand(1))
                         ->getType()
                         ->getScalarSizeInBits() <= 8)));
          if (!valid)
            break;
        }
        if (valid)
          trees.push_back(tree);
      }
    }
  }

  std::list<CandidateInst> candidates;
  for (auto tree : trees)
    candidates.push_back(tree.candidate);

  return candidates;
}

std::list<CandidateInst>
getSIMDableInstructions(BasicBlock &BB, const std::string &SIMDOp,
                        const unsigned int SIMDFactor,
                        const unsigned int SIMDDSPWidth) {
  if (SIMDOp == "add")
    return getSIMDableAdds(BB, SIMDFactor, SIMDDSPWidth);

  if (SIMDOp == "muladd")
    return getSIMDableMuladds(BB);

  return std::list<CandidateInst>();
}

void replaceMuladdsWithSIMDCall(const CandidateInst &treeA,
                                const CandidateInst &treeB,
                                Instruction *insertBefore, Function *MulAdd,
                                Function *ExtractProds, LLVMContext &context) {
  IRBuilder<> builder(insertBefore);

  SmallVector<Instruction *, 8> unpackedLeafsA;
  std::list<Instruction *> unpackedLeafsB;

  for (auto mulLeaf : treeB.inInsts)
    unpackedLeafsB.push_back(cast<Instruction>(mulLeaf));

  auto chainLenght = 0;
  Value *P = ConstantInt::get(IntegerType::get(context, 48), 0);
  SmallVector<Value *, 4> endsOfChain;
  for (auto mulLeafA : treeA.inInsts) {
    auto packed = false;
    auto mulLeafInstA = cast<Instruction>(mulLeafA);
    auto opA0 = mulLeafInstA->getOperand(0);
    auto opA1 = mulLeafInstA->getOperand(1);
    for (auto MI = unpackedLeafsB.begin(), ME = unpackedLeafsB.end(); MI != ME;
         ++MI) {
      auto mulLeafB = *MI;
      auto opB0 = mulLeafB->getOperand(0);
      auto opB1 = mulLeafB->getOperand(1);

      if ((opA0 == opB0) || (opA0 == opB1) || (opA1 == opB0) ||
          (opA1 == opB1)) {
        if ((chainLenght > 0) && ((chainLenght % 7) == 0)) {
          endsOfChain.push_back(builder.CreateCall(ExtractProds, P));
          P = ConstantInt::get(IntegerType::get(context, 48), 0);
        }
        opA0 = getUnextendedValue(opA0);
        opA1 = getUnextendedValue(opA1);
        opB0 = getUnextendedValue(opB0);
        opB1 = getUnextendedValue(opB1);
        // pack mulLeafA and mulLeafB + sum P
        // assign the result to P
        Value *A = (((opA0 != opB0) && (opA0 != opB1)) ? opA0 : opA1);
        Value *B = (((opB0 != opA0) && (opB0 != opA1)) ? opB0 : opB1);
        Value *D = (((opA0 == opB0) || (opA0 == opB1)) ? opA0 : opA1);
        Value *args[4] = {A, B, D, P};
        P = builder.CreateCall(MulAdd, args,
                               mulLeafA->getName() + "_" + mulLeafB->getName());
        chainLenght++;
        packed = true;
        unpackedLeafsB.erase(MI);
        break;
      }
    }
    if (!packed)
      unpackedLeafsA.push_back(mulLeafInstA);
  }

  // 1. call extractProds from P
  endsOfChain.push_back(builder.CreateCall(ExtractProds, P));

  // 2. sum the extracted prods to the unpacked leafs
  Value *sumA = ConstantInt::get(IntegerType::get(context, 48), 0);
  Value *sumB = ConstantInt::get(IntegerType::get(context, 48), 0);
  for (auto endOfChain : endsOfChain) {
    auto partialProdA = builder.CreateExtractValue(endOfChain, 0);
    sumA = builder.CreateAdd(
        sumA, builder.CreateSExt(partialProdA, IntegerType::get(context, 48)));
    auto partialProdB = builder.CreateExtractValue(endOfChain, 1);
    sumB = builder.CreateAdd(
        sumB, builder.CreateSExt(partialProdB, IntegerType::get(context, 48)));
  }
  // 3. replaceAllUsesWith sumA and sumB
  if (treeA.outInst->getType()->getScalarSizeInBits() < 48)
    sumA = builder.CreateTrunc(
        sumA, IntegerType::get(
                  context, treeA.outInst->getType()->getScalarSizeInBits()));
  treeA.outInst->replaceAllUsesWith(sumA);
  if (treeB.outInst->getType()->getScalarSizeInBits() < 48)
    sumB = builder.CreateTrunc(
        sumB, IntegerType::get(
                  context, treeB.outInst->getType()->getScalarSizeInBits()));
  treeB.outInst->replaceAllUsesWith(sumB);

  auto sumAName = treeA.outInst->getName();
  auto sumBName = treeB.outInst->getName();

  treeA.outInst->eraseFromParent();
  treeB.outInst->eraseFromParent();

  sumA->setName(sumAName);
  sumB->setName(sumBName);
}

void replaceAddsWithSIMDCall(SmallVector<CandidateInst, 4> instTuple,
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

void replaceInstsWithSIMDCall(SmallVector<CandidateInst, 4> instTuple,
                              Instruction *insertBefore, Function *SIMDFunc,
                              Function *SIMDFuncExtract,
                              const std::string SIMDOp,
                              const unsigned int SIMDFactor,
                              const unsigned int SIMDDSPWidth,
                              LLVMContext &context) {
  if (SIMDOp == "add") {
    replaceAddsWithSIMDCall(instTuple, insertBefore, SIMDFunc, SIMDFactor,
                            SIMDDSPWidth, context);
  } else if (SIMDOp == "muladd") {
    replaceMuladdsWithSIMDCall(instTuple[0], instTuple[1], insertBefore,
                               SIMDFunc, SIMDFuncExtract, context);
  }
}

bool SIMDAdd::runOnBasicBlock(BasicBlock &BB) {
  assert(((SIMDOp == "add") || (SIMDOp == "muladd")) &&
         "Unexpected value for simd-add-op option.");
  assert(((SIMDFactor == 2) || (SIMDFactor == 4)) &&
         "Unexpected value for simd-add-factor option.");
  assert((SIMDDSPWidth == 48) &&
         "Unexpected value for simd-add-dsp-width option.");

  Function *F = BB.getParent();
  if (F->getName().startswith("_ssdm_op") || F->getName().startswith("_simd"))
    return false;

  // Get the SIMD function
  Module *module = F->getParent();
  Function *SIMDFunc =
      module->getFunction("_simd_" + SIMDOp + "_" + std::to_string(SIMDFactor));
  assert(SIMDFunc && "SIMD function not found");

  Function *SIMDFuncExtract = nullptr;
  if (SIMDOp == "muladd") {
    SIMDFuncExtract = module->getFunction("_simd_" + SIMDOp + "_extract_" +
                                          std::to_string(SIMDFactor));
    assert(SIMDFuncExtract && "SIMD extract function not found");
  }

  LLVMContext &context = F->getContext();

  AA = &getAnalysis<AliasAnalysis>();

  std::list<CandidateInst> candidateInsts =
      getSIMDableInstructions(BB, SIMDOp, SIMDFactor, SIMDDSPWidth);

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
    SmallVector<CandidateInst, 4> instTuple;
    Instruction *lastDef = nullptr;
    Instruction *firstUse = nullptr;

    DenseMap<Instruction *, int> instMap;
    getInstMap(&BB, instMap);
    for (auto CI = candidateInsts.begin(), CE = candidateInsts.end();
         CI != CE;) {
      CandidateInst candidateInstCurr = *CI;
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

      if (SIMDOp == "muladd" && instTuple.size() > 0) {
        for (auto mulLeafA : candidateInstCurr.inInsts) {
          auto opA0 = dyn_cast<Instruction>(mulLeafA->getOperand(0));
          auto opA1 = dyn_cast<Instruction>(mulLeafA->getOperand(1));
          for (auto mulLeafB : instTuple[0].inInsts) {
            auto opB0 = dyn_cast<Instruction>(mulLeafB->getOperand(0));
            auto opB1 = dyn_cast<Instruction>(mulLeafB->getOperand(1));

            if ((opA0 == opB0) || (opA0 == opB1) || (opA1 == opB0) ||
                (opA1 == opB1)) {
              compatible = true;
              break;
            }
            if (compatible)
              break;
          }
        }
        if (!compatible) {
          CI++;
          continue;
        }
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

    if (instTuple.size() < 2)
      continue;

    replaceInstsWithSIMDCall(instTuple, firstUse, SIMDFunc, SIMDFuncExtract,
                             SIMDOp, SIMDFactor, SIMDDSPWidth, context);
    modified = true;
  }

  return modified;
}

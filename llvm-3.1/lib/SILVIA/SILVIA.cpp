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
#include "llvm/Support/raw_ostream.h"

#include <cassert>
#include <cmath>
#include <list>

using namespace llvm;

struct SILVIA : public BasicBlockPass {
  static char ID;
  SILVIA() : BasicBlockPass(ID) {}

  struct Candidate {
    SmallVector<Instruction *, 2> inInsts;
    Instruction *outInst;
  };

  struct DotProdTree {
    SILVIA::Candidate candidate;
    SmallVector<Instruction *, 8> addInternal;
  };

  bool runOnBasicBlock(BasicBlock &BB) override;

  virtual void getAnalysisUsage(AnalysisUsage &AU) const {
    AU.addRequired<AliasAnalysis>();
  }

  std::list<SILVIA::Candidate> getSIMDableInstructions(BasicBlock &BB);
  bool anticipateDefs(Instruction *inst, bool anticipateInst);
  bool posticipateUses(Instruction *inst, bool posticipateInst);
  Instruction *getFirstAliasingInst(Instruction *instToMove,
                                    Instruction *firstInst,
                                    Instruction *lastInst);
  Instruction *getLastAliasingInst(Instruction *instToMove,
                                   Instruction *firstInst,
                                   Instruction *lastInst);
  void replaceInstsWithSIMDCall(SmallVector<SILVIA::Candidate, 4> instTuple,
                                Instruction *insertBefore,
                                LLVMContext &context);

  AliasAnalysis *AA;
  Function *SIMDFunc;
  Function *SIMDFuncExtract;
};

char SILVIA::ID = 0;
static RegisterPass<SILVIA> X("silvia", "Pack instructions to SIMD DSPs",
                              false /* Only looks at CFG */,
                              true /* Transformation Pass */);

static cl::opt<std::string>
    SIMDOp("silvia-op", cl::init("add"), cl::Hidden,
           cl::desc("The operation to pack to SIMD DSPs. "
                    "Possible values are: add, muladd."));

static cl::opt<unsigned int>
    SIMDFactor("silvia-simd-factor", cl::init(4), cl::Hidden,
               cl::desc("The amount of operations to pack to SIMD DSPs."));

static cl::opt<unsigned int> DSPWidth("silvia-dsp-width", cl::init(48),
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

// Collect all the add instructions.
std::list<SILVIA::Candidate> getSIMDableAdds(BasicBlock &BB,
                                             const unsigned int SIMDFactor,
                                             const unsigned int DSPWidth) {
  std::list<SILVIA::Candidate> candidateInsts;

  const auto addMaxWidth = (DSPWidth / SIMDFactor);

  for (auto &I : BB) {
    if (I.getOpcode() != Instruction::Add)
      continue;
    if (isa<Constant>(I.getOperand(0)) || isa<Constant>(I.getOperand(1)))
      continue;
    if (I.getType()->getScalarSizeInBits() <= addMaxWidth) {
      SILVIA::Candidate candidate;
      candidate.inInsts.push_back(&I);
      candidate.outInst = &I;
      candidateInsts.push_back(candidate);
    }
  }

  return candidateInsts;
}

bool getDotProdTree(Instruction *addRoot, SILVIA::DotProdTree &tree) {
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

std::list<SILVIA::Candidate> getSIMDableMuladds(BasicBlock &BB) {
  SmallVector<SILVIA::DotProdTree, 8> trees;
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

      SILVIA::DotProdTree tree;
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

  std::list<SILVIA::Candidate> candidates;
  for (auto tree : trees)
    candidates.push_back(tree.candidate);

  return candidates;
}

std::list<SILVIA::Candidate> SILVIA::getSIMDableInstructions(BasicBlock &BB) {
  if (SIMDOp == "add")
    return getSIMDableAdds(BB, SIMDFactor, DSPWidth);

  if (SIMDOp == "muladd")
    return getSIMDableMuladds(BB);

  return std::list<SILVIA::Candidate>();
}

void replaceMuladdsWithSIMDCall(const SILVIA::Candidate &treeA,
                                const SILVIA::Candidate &treeB,
                                Instruction *insertBefore, Function *MulAdd,
                                Function *ExtractProds, LLVMContext &context) {
  IRBuilder<> builder(insertBefore);

  SmallVector<Instruction *, 8> unpackedLeafsA;
  SmallVector<Instruction *, 8> unpackedLeafsB;

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
        for (auto i = 0; i < 4; ++i) {
          if (args[i]->getType()->getScalarSizeInBits() < 8) {
            args[i] = builder.CreateSExt(args[i], IntegerType::get(context, 8),
                                         args[i]->getName() + "_sext");
          }
        }
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
  Value *sumA = nullptr;
  Value *sumB = nullptr;
  for (auto i = 0; i < endsOfChain.size(); ++i) {
    const auto partialProdSize = unsigned(18 + std::ceil(std::log2(i + 1)));
    auto partialProdA = builder.CreateExtractValue(endsOfChain[i], 0);
    auto partialProdB = builder.CreateExtractValue(endsOfChain[i], 1);
    if (partialProdSize > 18) {
      partialProdA = builder.CreateSExt(
          partialProdA, IntegerType::get(context, partialProdSize));
      partialProdB = builder.CreateSExt(
          partialProdB, IntegerType::get(context, partialProdSize));
    }
    if (!sumA) {
      sumA = partialProdA;
      sumB = partialProdB;
    } else {
      if (sumA->getType()->getScalarSizeInBits() < partialProdSize) {
        sumA = builder.CreateSExt(sumA,
                                  IntegerType::get(context, partialProdSize));
        sumB = builder.CreateSExt(sumB,
                                  IntegerType::get(context, partialProdSize));
      }
      sumA = builder.CreateAdd(sumA, partialProdA);
      sumB = builder.CreateAdd(sumB, partialProdB);
    }
  }
  // Sum the unpacked leafs.
  for (auto i = 0; i < unpackedLeafsA.size(); ++i) {
    Value *unpackedLeafA = unpackedLeafsA[i];
    auto unpackedLeafASize = unpackedLeafA->getType()->getScalarSizeInBits();
    const auto partialProdSize =
        unsigned(18 + std::ceil(std::log2(i + 1 + endsOfChain.size())));
    if (unpackedLeafASize < partialProdSize) {
      unpackedLeafA = builder.CreateSExt(
          unpackedLeafA, IntegerType::get(context, partialProdSize));
    } else if (unpackedLeafASize > partialProdSize) {
      unpackedLeafA = builder.CreateTrunc(
          unpackedLeafA, IntegerType::get(context, partialProdSize));
    }
    sumA = builder.CreateAdd(sumA, unpackedLeafA);
  }

  for (auto i = 0; i < unpackedLeafsB.size(); ++i) {
    Value *unpackedLeafB = unpackedLeafsB[i];
    auto unpackedLeafBSize = unpackedLeafB->getType()->getScalarSizeInBits();
    const auto partialProdSize =
        unsigned(18 + std::ceil(std::log2(i + 1 + endsOfChain.size())));
    if (unpackedLeafBSize < partialProdSize) {
      unpackedLeafB = builder.CreateSExt(
          unpackedLeafB, IntegerType::get(context, partialProdSize));
    } else if (unpackedLeafBSize > partialProdSize) {
      unpackedLeafB = builder.CreateTrunc(
          unpackedLeafB, IntegerType::get(context, partialProdSize));
    }
    sumB = builder.CreateAdd(sumB, unpackedLeafB);
  }
  // 3. replaceAllUsesWith sumA and sumB
  const auto rootASize = treeA.outInst->getType()->getScalarSizeInBits();
  if (rootASize < sumA->getType()->getScalarSizeInBits())
    sumA = builder.CreateTrunc(sumA, IntegerType::get(context, rootASize));
  else if (rootASize > sumA->getType()->getScalarSizeInBits())
    sumA = builder.CreateSExt(sumA, IntegerType::get(context, rootASize));
  treeA.outInst->replaceAllUsesWith(sumA);
  const auto rootBSize = treeB.outInst->getType()->getScalarSizeInBits();
  if (rootBSize < sumB->getType()->getScalarSizeInBits())
    sumB = builder.CreateTrunc(sumB, IntegerType::get(context, rootBSize));
  else if (rootBSize > sumB->getType()->getScalarSizeInBits())
    sumB = builder.CreateSExt(sumB, IntegerType::get(context, rootBSize));
  treeB.outInst->replaceAllUsesWith(sumB);

  auto rootAName = treeA.outInst->getName();
  auto rootBName = treeB.outInst->getName();

  treeA.outInst->eraseFromParent();
  treeB.outInst->eraseFromParent();

  sumA->setName(rootAName);
  sumB->setName(rootBName);
}

void replaceAddsWithSIMDCall(SmallVector<SILVIA::Candidate, 4> instTuple,
                             Instruction *insertBefore, Function *SIMDFunc,
                             const unsigned int SIMDFactor,
                             const unsigned int DSPWidth,
                             LLVMContext &context) {
  IRBuilder<> builder(insertBefore);

  const auto dataBitWidth = (DSPWidth / SIMDFactor);

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

void SILVIA::replaceInstsWithSIMDCall(
    SmallVector<SILVIA::Candidate, 4> instTuple, Instruction *insertBefore,
    LLVMContext &context) {
  if (SIMDOp == "add") {
    replaceAddsWithSIMDCall(instTuple, insertBefore, SIMDFunc, SIMDFactor,
                            DSPWidth, context);
  } else if (SIMDOp == "muladd") {
    replaceMuladdsWithSIMDCall(instTuple[0], instTuple[1], insertBefore,
                               SIMDFunc, SIMDFuncExtract, context);
  }
}

bool SILVIA::runOnBasicBlock(BasicBlock &BB) {
  assert(((SIMDOp == "add") || (SIMDOp == "muladd")) &&
         "Unexpected value for silvia-op option.");
  assert(((SIMDFactor == 2) || (SIMDFactor == 4)) &&
         "Unexpected value for silvia-simd-factor option.");
  assert((DSPWidth == 48) && "Unexpected value for silvia-dsp-width option.");

  Function *F = BB.getParent();
  if (F->getName().startswith("_ssdm_op") || F->getName().startswith("_simd"))
    return false;

  // Get the SIMD function
  Module *module = F->getParent();
  SIMDFunc =
      module->getFunction("_simd_" + SIMDOp + "_" + std::to_string(SIMDFactor));
  assert(SIMDFunc && "SIMD function not found");

  SIMDFuncExtract = nullptr;
  if (SIMDOp == "muladd") {
    SIMDFuncExtract = module->getFunction("_simd_" + SIMDOp + "_extract_" +
                                          std::to_string(SIMDFactor));
    assert(SIMDFuncExtract && "SIMD extract function not found");
  }

  LLVMContext &context = F->getContext();

  AA = &getAnalysis<AliasAnalysis>();

  std::list<SILVIA::Candidate> candidateInsts = getSIMDableInstructions(BB);

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

    replaceInstsWithSIMDCall(instTuple, firstUse, context);
    modified = true;
  }

  return modified;
}
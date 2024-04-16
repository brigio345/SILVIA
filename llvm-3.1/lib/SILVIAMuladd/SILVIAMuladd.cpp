#include "llvm/ADT/SmallVector.h"
#include "llvm/BasicBlock.h"
#include "llvm/Function.h"
#include "llvm/Module.h"
#include "llvm/Pass.h"
#include "llvm/Support/IRBuilder.h"

#include <cassert>
#include <list>

#include "../SILVIA/SILVIA.h"

using namespace llvm;

struct SILVIAMuladd : public SILVIA {
  static char ID;
  SILVIAMuladd() : SILVIA(ID) {}

  bool runOnBasicBlock(BasicBlock &BB) override;

  std::list<SILVIA::Candidate> getSIMDableInstructions(BasicBlock &BB) override;
  bool isCandidateCompatibleWithTuple(
      SILVIA::Candidate &candidate,
      SmallVector<SILVIA::Candidate, 4> &tuple) override;
  bool isTupleFull(SmallVector<SILVIA::Candidate, 4> &tuple) override;
  void replaceInstsWithSIMDCall(SmallVector<SILVIA::Candidate, 4> instTuple,
                                Instruction *insertBefore,
                                LLVMContext &context) override;

  Function *MulAdd;
  Function *ExtractProds;
};

char SILVIAMuladd::ID = 0;
static RegisterPass<SILVIAMuladd> X("silvia-muladd",
                                    "Pack muladds to SIMD DSPs",
                                    false /* Only looks at CFG */,
                                    true /* Transformation Pass */);

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

std::list<SILVIA::Candidate>
SILVIAMuladd::getSIMDableInstructions(BasicBlock &BB) {
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

bool SILVIAMuladd::isCandidateCompatibleWithTuple(
    SILVIA::Candidate &candidate, SmallVector<SILVIA::Candidate, 4> &tuple) {
  if (tuple.size() < 1)
    return true;

  for (auto mulLeafA : candidate.inInsts) {
    auto opA0 = dyn_cast<Instruction>(mulLeafA->getOperand(0));
    auto opA1 = dyn_cast<Instruction>(mulLeafA->getOperand(1));
    for (auto mulLeafB : tuple[0].inInsts) {
      auto opB0 = dyn_cast<Instruction>(mulLeafB->getOperand(0));
      auto opB1 = dyn_cast<Instruction>(mulLeafB->getOperand(1));

      if ((opA0 == opB0) || (opA0 == opB1) || (opA1 == opB0) || (opA1 == opB1))
        return true;
    }
  }

  return false;
}

bool SILVIAMuladd::isTupleFull(SmallVector<SILVIA::Candidate, 4> &tuple) {
  return (tuple.size() == 2);
}

void SILVIAMuladd::replaceInstsWithSIMDCall(
    SmallVector<SILVIA::Candidate, 4> instTuple, Instruction *insertBefore,
    LLVMContext &context) {
  const auto treeA = instTuple[0];
  const auto treeB = instTuple[1];

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

bool SILVIAMuladd::runOnBasicBlock(BasicBlock &BB) {
  // Get the SIMD function
  Module *module = BB.getParent()->getParent();
  MulAdd = module->getFunction("_simd_muladd_2");
  assert(MulAdd && "SIMD function not found");

  ExtractProds = module->getFunction("_simd_muladd_extract_2");
  assert(ExtractProds && "SIMD extract function not found");

  return SILVIA::runOnBasicBlock(BB);
}

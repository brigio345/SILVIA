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

  struct AddTree {
    SILVIA::Candidate candidate;
    SmallVector<Instruction *, 8> addInternal;
  };

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
  Function *ExtractProdsSign;
  Function *ExtractProdsUnsign;
};

char SILVIAMuladd::ID = 0;
static RegisterPass<SILVIAMuladd> X("silvia-muladd",
                                    "Pack muladds to SIMD DSPs",
                                    false /* Only looks at CFG */,
                                    true /* Transformation Pass */);

struct LeavesPack {
  SmallVector<Value *, 3> leaves;
  std::string name;
};

Value *getUnextendedValue(Value *V) {
  if (auto I = dyn_cast<Instruction>(V)) {
    if ((I->getOpcode() == Instruction::SExt) ||
        (I->getOpcode() == Instruction::ZExt))
      return getUnextendedValue(I->getOperand(0));
  }

  return V;
}

int getExtOpcode(Instruction *I) {
  for (auto UI = I->use_begin(), UE = I->use_end(); UI != UE; ++UI) {
    Instruction *user = dyn_cast<Instruction>(*UI);
    auto userOpcode = user->getOpcode();
    if ((userOpcode == Instruction::SExt) || (userOpcode == Instruction::ZExt))
      return userOpcode;
  }

  return -1;
}

void getAddTree(Instruction *root, SILVIAMuladd::AddTree &tree) {
  if (root->getOpcode() != Instruction::Add) {
    tree.candidate.inVals.push_back(root);
    return;
  }

  for (unsigned i = 0; i < root->getNumOperands(); ++i) {
    auto op = root->getOperand(i);

    auto opInst = dyn_cast<Instruction>(op);
    auto hasOneUse = op->hasOneUse();
    while (hasOneUse && opInst &&
           ((opInst->getOpcode() == Instruction::SExt) ||
            (opInst->getOpcode() == Instruction::ZExt))) {
      op = opInst->getOperand(0);
      opInst = dyn_cast<Instruction>(op);

      hasOneUse &= op->hasOneUse();
    }

    if ((!hasOneUse) || (!opInst)) {
      tree.candidate.inVals.push_back(op);
      continue;
    }

    // The tree cannot absorb an op with multiple uses, since its value is
    // needed elsewhere too.
    // TODO: Accept multiple uses too. We need to split the chain in that point.
    if (opInst->hasOneUse() && (opInst->getOpcode() == Instruction::Add)) {
      getAddTree(opInst, tree);
      tree.addInternal.push_back(opInst);
    } else {
      tree.candidate.inVals.push_back(op);
    }
  }

  tree.candidate.outInst = root;
}

std::list<SILVIA::Candidate>
SILVIAMuladd::getSIMDableInstructions(BasicBlock &BB) {
  SmallVector<SILVIAMuladd::AddTree, 8> trees;
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

      SILVIAMuladd::AddTree tree;
      getAddTree(I, tree);

      auto validMuls = 0;
      for (auto leaf : tree.candidate.inVals) {
        if (auto leafInst = dyn_cast<Instruction>(leaf)) {
          if (leafInst->getOpcode() != Instruction::Mul)
            continue;

          validMuls += ((getUnextendedValue(leafInst->getOperand(0))
                             ->getType()
                             ->getScalarSizeInBits() <= 8) &&
                        ((getUnextendedValue(leafInst->getOperand(1))
                              ->getType()
                              ->getScalarSizeInBits() <= 8)));
        }
      }

      if (validMuls > 1)
        trees.push_back(tree);
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

  for (auto mulLeafA : candidate.inVals) {
    auto mulLeafInstA = dyn_cast<Instruction>(mulLeafA);

    if ((!mulLeafInstA) || (mulLeafInstA->getOpcode() != Instruction::Mul))
      continue;

    // TODO: Check if the dyn_casts do not return nullptr.
    auto opA0 =
        getUnextendedValue(dyn_cast<Instruction>(mulLeafInstA->getOperand(0)));
    auto opA1 =
        getUnextendedValue(dyn_cast<Instruction>(mulLeafInstA->getOperand(1)));
    if ((opA0->getType()->getScalarSizeInBits() > 8) ||
        (opA1->getType()->getScalarSizeInBits() > 8))
      continue;
    for (auto mulLeafB : tuple[0].inVals) {
      auto mulLeafInstB = dyn_cast<Instruction>(mulLeafB);

      if ((!mulLeafInstB) || (mulLeafInstB->getOpcode() != Instruction::Mul))
        continue;
      // TODO: Check if the dyn_casts do not return nullptr.
      auto opB0 = getUnextendedValue(
          dyn_cast<Instruction>(mulLeafInstB->getOperand(0)));
      auto opB1 = getUnextendedValue(
          dyn_cast<Instruction>(mulLeafInstB->getOperand(1)));

      if ((opB0->getType()->getScalarSizeInBits() > 8) ||
          (opB1->getType()->getScalarSizeInBits() > 8))
        continue;

      if ((opA0 == opB0) || (opA0 == opB1) || (opA1 == opB0) || (opA1 == opB1))
        return true;
    }
  }

  return false;
}

bool SILVIAMuladd::isTupleFull(SmallVector<SILVIA::Candidate, 4> &tuple) {
  return (tuple.size() == 2);
}

unsigned getMaxChainLength(const SmallVector<LeavesPack, 8> &leavesPacks,
                           const int DExt) {
  unsigned maxSize[3] = {0};

  for (auto leavesPack : leavesPacks) {
    for (int i = 0; i < 3; ++i) {
      const auto size = getUnextendedValue(leavesPack.leaves[i])
                            ->getType()
                            ->getScalarSizeInBits();
      if (size > maxSize[i])
        maxSize[i] = size;
    }
  }

  const auto q = 18;
  const auto m = ((maxSize[0] > maxSize[1]) ? maxSize[0] : maxSize[1]);
  const auto n = maxSize[2];

  const unsigned N =
      ((DExt == Instruction::SExt)
           ? (((1 << (q - 1)) - 1) / float(1 << (m + n - 2)))
           : (((1 << q) - 1) / float(((1 << m) - 1) * ((1 << n) - 1))));

  return N;
}

void SILVIAMuladd::replaceInstsWithSIMDCall(
    SmallVector<SILVIA::Candidate, 4> instTuple, Instruction *insertBefore,
    LLVMContext &context) {
  const auto treeA = instTuple[0];
  const auto treeB = instTuple[1];

  IRBuilder<> builder(insertBefore);

  SmallVector<Value *, 8> unpackedLeafsA;
  SmallVector<Instruction *, 8> unpackedMulsA;
  SmallVector<Value *, 8> unpackedLeafsB;
  SmallVector<Instruction *, 8> unpackedMulsB;

  for (auto leaf : treeA.inVals) {
    auto leafInst = dyn_cast<Instruction>(leaf);

    if ((leafInst) && (leafInst->getOpcode() == Instruction::Mul) &&
        ((getUnextendedValue(leafInst->getOperand(0))
              ->getType()
              ->getScalarSizeInBits() <= 8) &&
         (getUnextendedValue(leafInst->getOperand(1))
              ->getType()
              ->getScalarSizeInBits() <= 8)))
      unpackedMulsA.push_back(leafInst);
    else
      unpackedLeafsA.push_back(leaf);
  }

  for (auto leaf : treeB.inVals) {
    auto leafInst = dyn_cast<Instruction>(leaf);

    if ((leafInst) && (leafInst->getOpcode() == Instruction::Mul) &&
        ((getUnextendedValue(leafInst->getOperand(0))
              ->getType()
              ->getScalarSizeInBits() <= 8) &&
         (getUnextendedValue(leafInst->getOperand(1))
              ->getType()
              ->getScalarSizeInBits() <= 8)))
      unpackedMulsB.push_back(leafInst);
    else
      unpackedLeafsB.push_back(leaf);
  }

  SmallVector<LeavesPack, 8> leavesPacks;
  int extA;
  int extB;
  for (auto mulLeafA : unpackedMulsA) {
    auto packed = false;
    auto opA0 = mulLeafA->getOperand(0);
    auto opA1 = mulLeafA->getOperand(1);
    for (auto MI = unpackedMulsB.begin(), ME = unpackedMulsB.end(); MI != ME;
         ++MI) {
      auto mulLeafB = *MI;
      auto opB0 = mulLeafB->getOperand(0);
      auto opB1 = mulLeafB->getOperand(1);

      if ((opA0 == opB0) || (opA0 == opB1) || (opA1 == opB0) ||
          (opA1 == opB1)) {
        if (leavesPacks.size() == 0) {
          extA = getExtOpcode(mulLeafA);
          extB = getExtOpcode(mulLeafB);
        } else if ((getExtOpcode(mulLeafA) != extA) ||
                   (getExtOpcode(mulLeafB) != extB)) {
          return;
        }

        LeavesPack pack;
        pack.leaves.push_back(((opA0 != opB0) && (opA0 != opB1)) ? opA0 : opA1);
        pack.leaves.push_back(((opB0 != opA0) && (opB0 != opA1)) ? opB0 : opB1);
        pack.leaves.push_back(((opA0 == opB0) || (opA0 == opB1)) ? opA0 : opA1);
        pack.name = std::string(mulLeafA->getName()) + "_" +
                    std::string(mulLeafB->getName());

        leavesPacks.push_back(pack);
        packed = true;
        unpackedMulsB.erase(MI);

        break;
      }
    }
    if (!packed)
      unpackedLeafsA.push_back(mulLeafA);
  }

  for (auto mul : unpackedMulsB)
    unpackedLeafsB.push_back(mul);

  const auto maxChainLength = getMaxChainLength(leavesPacks, extB);
  auto ExtractProds =
      ((extB == Instruction::SExt) ? ExtractProdsSign : ExtractProdsUnsign);
  Value *P = ConstantInt::get(IntegerType::get(context, 36), 0);
  SmallVector<Value *, 4> endsOfChain;
  for (int dspID = 0; dspID < leavesPacks.size(); ++dspID) {
    if ((dspID > 0) && ((dspID % maxChainLength) == 0)) {
      endsOfChain.push_back(builder.CreateCall(ExtractProds, P));
      P = ConstantInt::get(IntegerType::get(context, 36), 0);
    }
    // pack mulLeafA and mulLeafB + sum P
    // assign the result to P
    Value *args[4] = {leavesPacks[dspID].leaves[0],
                      leavesPacks[dspID].leaves[1],
                      leavesPacks[dspID].leaves[2], P};
    auto MulAddTy = cast<FunctionType>(
        cast<PointerType>(MulAdd->getType())->getElementType());
    for (auto i = 0; i < 3; ++i) {
      auto argSize = MulAddTy->getParamType(i)->getScalarSizeInBits();
      if (args[i]->getType()->getScalarSizeInBits() > argSize) {
        auto argOrig = args[i];
        args[i] = getUnextendedValue(args[i]);
        if (args[i]->getType()->getScalarSizeInBits() < argSize) {
          args[i] =
              builder.CreateTrunc(argOrig, IntegerType::get(context, argSize),
                                  argOrig->getName() + "_trunc");
        }
      }
      // This check evaluates to true when the precision of the mul operation
      // is actually less or equal to 8 bits.
      // The result is the same with both sext and zext since the higher bits
      // are ignored.
      if (args[i]->getType()->getScalarSizeInBits() < argSize) {
        args[i] =
            builder.CreateZExt(args[i], IntegerType::get(context, argSize),
                               args[i]->getName() + "_zext");
      }
    }
    P = builder.CreateCall(MulAdd, args, leavesPacks[dspID].name);
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
      partialProdA =
          ((extA == Instruction::SExt)
               ? builder.CreateSExt(partialProdA,
                                    IntegerType::get(context, partialProdSize))
               : builder.CreateZExt(
                     partialProdA, IntegerType::get(context, partialProdSize)));
      partialProdB =
          ((extB == Instruction::SExt)
               ? builder.CreateSExt(partialProdB,
                                    IntegerType::get(context, partialProdSize))
               : builder.CreateZExt(
                     partialProdB, IntegerType::get(context, partialProdSize)));
    }
    if (!sumA) {
      sumA = partialProdA;
      sumB = partialProdB;
    } else {
      if (sumA->getType()->getScalarSizeInBits() < partialProdSize) {
        sumA = ((extA == Instruction::SExt)
                    ? builder.CreateSExt(
                          sumA, IntegerType::get(context, partialProdSize))
                    : builder.CreateZExt(
                          sumA, IntegerType::get(context, partialProdSize)));
        sumB = ((extB == Instruction::SExt)
                    ? builder.CreateSExt(
                          sumB, IntegerType::get(context, partialProdSize))
                    : builder.CreateZExt(
                          sumB, IntegerType::get(context, partialProdSize)));
      }
      sumA = builder.CreateAdd(sumA, partialProdA);
      sumB = builder.CreateAdd(sumB, partialProdB);
    }
  }
  // Sum the unpacked leafs.
  // TODO: Maybe the partial sums should have higher bitwidth to reduce
  // overflow risk with signed numbers.
  const auto rootASize = treeA.outInst->getType()->getScalarSizeInBits();
  if (rootASize < sumA->getType()->getScalarSizeInBits()) {
    sumA = builder.CreateTrunc(sumA, IntegerType::get(context, rootASize));
  } else if (rootASize > sumA->getType()->getScalarSizeInBits()) {
    sumA =
        ((extA == Instruction::SExt)
             ? builder.CreateSExt(sumA, IntegerType::get(context, rootASize))
             : builder.CreateZExt(sumA, IntegerType::get(context, rootASize)));
  }
  const auto rootBSize = treeB.outInst->getType()->getScalarSizeInBits();
  if (rootBSize < sumB->getType()->getScalarSizeInBits()) {
    sumB = builder.CreateTrunc(sumB, IntegerType::get(context, rootBSize));
  } else if (rootBSize > sumB->getType()->getScalarSizeInBits()) {
    sumB =
        ((extB == Instruction::SExt)
             ? builder.CreateSExt(sumB, IntegerType::get(context, rootBSize))
             : builder.CreateZExt(sumB, IntegerType::get(context, rootBSize)));
  }

  for (auto i = 0; i < unpackedLeafsA.size(); ++i) {
    Value *unpackedLeafA = unpackedLeafsA[i];
    auto unpackedLeafASize = unpackedLeafA->getType()->getScalarSizeInBits();
    if (unpackedLeafASize < rootASize) {
      unpackedLeafA =
          ((extA == Instruction::SExt)
               ? builder.CreateSExt(unpackedLeafA,
                                    IntegerType::get(context, rootASize))
               : builder.CreateZExt(unpackedLeafA,
                                    IntegerType::get(context, rootASize)));
    } else if (unpackedLeafASize > rootASize) {
      unpackedLeafA = builder.CreateTrunc(unpackedLeafA,
                                          IntegerType::get(context, rootASize));
    }

    sumA = builder.CreateAdd(sumA, unpackedLeafA);
  }

  for (auto i = 0; i < unpackedLeafsB.size(); ++i) {
    Value *unpackedLeafB = unpackedLeafsB[i];
    auto unpackedLeafBSize = unpackedLeafB->getType()->getScalarSizeInBits();
    if (unpackedLeafBSize < rootBSize) {
      unpackedLeafB =
          ((extB == Instruction::SExt)
               ? builder.CreateSExt(unpackedLeafB,
                                    IntegerType::get(context, rootBSize))
               : builder.CreateZExt(unpackedLeafB,
                                    IntegerType::get(context, rootBSize)));
    } else if (unpackedLeafBSize > rootBSize) {
      unpackedLeafB = builder.CreateTrunc(unpackedLeafB,
                                          IntegerType::get(context, rootBSize));
    }

    sumB = builder.CreateAdd(sumB, unpackedLeafB);
  }
  sumA = builder.CreateTrunc(
      sumA, IntegerType::get(context,
                             treeA.outInst->getType()->getScalarSizeInBits()));
  sumB = builder.CreateTrunc(
      sumB, IntegerType::get(context,
                             treeB.outInst->getType()->getScalarSizeInBits()));
  // 3. replaceAllUsesWith sumA and sumB
  treeA.outInst->replaceAllUsesWith(sumA);
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

  ExtractProdsSign = module->getFunction("_simd_muladd_signed_extract_2");
  ExtractProdsUnsign = module->getFunction("_simd_muladd_unsigned_extract_2");
  assert((ExtractProdsSign && ExtractProdsUnsign) &&
         "SIMD extract function not found");

  return SILVIA::runOnBasicBlock(BB);
}

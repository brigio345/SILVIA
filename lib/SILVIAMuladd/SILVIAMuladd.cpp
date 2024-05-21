#include "llvm/ADT/SmallVector.h"
#include "llvm/BasicBlock.h"
#include "llvm/Function.h"
#include "llvm/Module.h"
#include "llvm/Pass.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/IRBuilder.h"
#include "llvm/Transforms/Utils/Cloning.h"

#include <cassert>
#include <list>

#include "SILVIA/SILVIA.h"

using namespace llvm;

struct SILVIAMuladd : public SILVIA {
  static char ID;
  SILVIAMuladd() : SILVIA(ID) {}

  struct AddTree {
    SILVIA::Candidate candidate;
    SmallVector<Instruction *, 8> addInternal;
  };

  bool runOnBasicBlock(BasicBlock &BB) override;

  bool doInitialization(Module &M) override {
#ifdef DEBUG
    packedTrees = 0;
    packedLeaves = 0;
#endif /* DEBUG */
    return false;
  }

  bool doFinalization(Module &M) override {
#ifdef DEBUG
    if (packedTrees > 0)
      dbgs() << "SILVIAMuladd::doFinalization: packed " << packedTrees
             << " trees (" << packedLeaves << " leaves).\n";
#endif /* DEBUG */
    return false;
  }

  std::list<SILVIA::Candidate> getCandidates(BasicBlock &BB) override;
  bool isCandidateCompatibleWithTuple(
      SILVIA::Candidate &candidate,
      SmallVector<SILVIA::Candidate, 4> &tuple) override;
  bool isTupleFull(SmallVector<SILVIA::Candidate, 4> &tuple) override;
  Value *packTuple(SmallVector<SILVIA::Candidate, 4> instTuple,
                   Instruction *insertBefore, LLVMContext &context) override;

  Function *MulAdd;
  Function *ExtractProdsSign;
  Function *ExtractProdsUnsign;
#ifdef DEBUG
  unsigned long packedTrees;
  unsigned long packedLeaves;
#endif /* DEBUG */
};

char SILVIAMuladd::ID = 0;
static RegisterPass<SILVIAMuladd> X("silvia-muladd",
                                    "Pack muladds to SIMD DSPs",
                                    false /* Only looks at CFG */,
                                    true /* Transformation Pass */);

static cl::opt<unsigned int>
    SILVIAMuladdOpSize("silvia-muladd-op-size", cl::init(8), cl::Hidden,
                       cl::desc("The size in bits of the multiplicands."));

static cl::opt<int>
    SILVIAMuladdMaxChainLen("silvia-muladd-max-chain-len", cl::init(-1),
                            cl::Hidden,
                            cl::desc("The maximum length of a chain of DSPs."));

static cl::opt<bool>
    SILVIAMuladdInline("silvia-muladd-inline", cl::init(false), cl::Hidden,
                       cl::desc("Whether to inline the packed operations."));

struct LeavesPack {
  SmallVector<Value *, 3> leaves;
  std::string name;
};

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
      DEBUG(dbgs() << "SILVIAMuladd::getAddTree: leaf detected because operand "
                   << ((!hasOneUse) ? "has many uses.\n"
                                    : "is not an Instruction.\n"));
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
      DEBUG(dbgs() << "SILVIAMuladd::getAddTree: leaf detected because operand "
                   << ((!hasOneUse) ? "has many uses.\n"
                                    : "is not an add (" +
                                          std::string(opInst->getOpcodeName()) +
                                          ").\n"));
      tree.candidate.inVals.push_back(op);
    }
  }

  tree.candidate.outInst = root;
}

std::list<SILVIA::Candidate> SILVIAMuladd::getCandidates(BasicBlock &BB) {
  SmallVector<SILVIAMuladd::AddTree, 8> trees;
  // Iterate in reverse order to avoid collecting subset trees.
  for (auto II = BB.end(), IB = BB.begin(); II != IB; --II) {
    Instruction *I = II;
    if (I->getOpcode() == Instruction::Add) {
      bool subset = false;
      for (const auto &tree : trees) {
        for (const auto &add : tree.addInternal) {
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
#ifdef DEBUG
      auto muls = 0;
#endif /* DEBUG */
      for (const auto &leaf : tree.candidate.inVals) {
        if (auto leafInst = dyn_cast<Instruction>(leaf)) {
          if (leafInst->getOpcode() != Instruction::Mul)
            continue;

          DEBUG(muls++);
          validMuls += ((SILVIA::getUnextendedValue(leafInst->getOperand(0))
                             ->getType()
                             ->getScalarSizeInBits() <= SILVIAMuladdOpSize) &&
                        ((SILVIA::getUnextendedValue(leafInst->getOperand(1))
                              ->getType()
                              ->getScalarSizeInBits() <= SILVIAMuladdOpSize)));
        }
      }

      DEBUG(if (muls > 0) dbgs()
            << "SILVIAMuladd::getCandidates: found a tree with " << muls
            << " muls (" << validMuls << " valid).\n");
      if (validMuls > 1)
        trees.push_back(tree);
    }
  }

  std::list<SILVIA::Candidate> candidates;
  for (const auto &tree : trees)
    candidates.push_back(tree.candidate);

  return candidates;
}

bool SILVIAMuladd::isCandidateCompatibleWithTuple(
    SILVIA::Candidate &candidate, SmallVector<SILVIA::Candidate, 4> &tuple) {
  if (tuple.size() < 1)
    return true;

  for (const auto &mulLeafA : candidate.inVals) {
    auto mulLeafInstA = dyn_cast<Instruction>(mulLeafA);

    if ((!mulLeafInstA) || (mulLeafInstA->getOpcode() != Instruction::Mul))
      continue;

    // TODO: Check if the dyn_casts do not return nullptr.
    auto opA0 = SILVIA::getUnextendedValue(
        dyn_cast<Instruction>(mulLeafInstA->getOperand(0)));
    auto opA1 = SILVIA::getUnextendedValue(
        dyn_cast<Instruction>(mulLeafInstA->getOperand(1)));
    if ((opA0->getType()->getScalarSizeInBits() > SILVIAMuladdOpSize) ||
        (opA1->getType()->getScalarSizeInBits() > SILVIAMuladdOpSize))
      continue;
    for (const auto &mulLeafB : tuple[0].inVals) {
      auto mulLeafInstB = dyn_cast<Instruction>(mulLeafB);

      if ((!mulLeafInstB) || (mulLeafInstB->getOpcode() != Instruction::Mul))
        continue;
      // TODO: Check if the dyn_casts do not return nullptr.
      auto opB0 = SILVIA::getUnextendedValue(
          dyn_cast<Instruction>(mulLeafInstB->getOperand(0)));
      auto opB1 = SILVIA::getUnextendedValue(
          dyn_cast<Instruction>(mulLeafInstB->getOperand(1)));

      if ((opB0->getType()->getScalarSizeInBits() > SILVIAMuladdOpSize) ||
          (opB1->getType()->getScalarSizeInBits() > SILVIAMuladdOpSize))
        continue;

      if ((opA0 == opB0) || (opA0 == opB1) || (opA1 == opB0) || (opA1 == opB1))
        return true;
    }
  }

  DEBUG(dbgs()
        << "SILVIAMuladd::isCandidateCompatibleWithTuple: candidate rooted in "
        << candidate.outInst->getName()
        << " does not share an operand with candidate rooted in "
        << tuple[0].outInst->getName() << ".\n");
  return false;
}

bool SILVIAMuladd::isTupleFull(SmallVector<SILVIA::Candidate, 4> &tuple) {
  return (tuple.size() == 2);
}

unsigned getMaxChainLength(const SmallVector<LeavesPack, 8> &leavesPacks,
                           const int DExt) {
  unsigned maxSize[3] = {0};

  for (const auto &leavesPack : leavesPacks) {
    for (int i = 0; i < 3; ++i) {
      const auto size = SILVIA::getUnextendedValue(leavesPack.leaves[i])
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

Value *buildAddTree(SmallVector<Value *, 8> &addends, int ext,
                    IRBuilder<> &builder, LLVMContext &context) {
  if (addends.size() == 0)
    return nullptr;

  SmallVector<Value *, 8> lowerAddends;
  for (unsigned i = 0; i < addends.size(); i += 2) {
    auto A = addends[i];
    auto B = (((i + 1) < addends.size()) ? addends[i + 1] : nullptr);

    if (!B) {
      lowerAddends.push_back(A);
      break;
    }

    auto ASize = A->getType()->getScalarSizeInBits();
    auto BSize = B->getType()->getScalarSizeInBits();
    auto addSize = (((ASize > BSize) ? ASize : BSize) + 1);
    A = ((ext == Instruction::SExt)
             ? builder.CreateSExt(A, IntegerType::get(context, addSize))
             : builder.CreateZExt(A, IntegerType::get(context, addSize)));
    B = ((ext == Instruction::SExt)
             ? builder.CreateSExt(B, IntegerType::get(context, addSize))
             : builder.CreateZExt(B, IntegerType::get(context, addSize)));

    lowerAddends.push_back(builder.CreateAdd(A, B));
  }

  return ((lowerAddends.size() == 1)
              ? lowerAddends[0]
              : buildAddTree(lowerAddends, ext, builder, context));
}

Value *SILVIAMuladd::packTuple(SmallVector<SILVIA::Candidate, 4> instTuple,
                               Instruction *insertBefore,
                               LLVMContext &context) {
  IRBuilder<> builder(insertBefore);

  SmallVector<SmallVector<Value *, 8>, 2> toAdd(instTuple.size());
  SmallVector<SmallVector<Instruction *, 8>, 2> unpackedMuls(instTuple.size());

  for (unsigned i = 0; i < instTuple.size(); ++i) {
    for (const auto &leaf : instTuple[i].inVals) {
      auto leafInst = dyn_cast<Instruction>(leaf);

      if ((leafInst) && (leafInst->getOpcode() == Instruction::Mul) &&
          ((SILVIA::getUnextendedValue(leafInst->getOperand(0))
                ->getType()
                ->getScalarSizeInBits() <= SILVIAMuladdOpSize) &&
           (SILVIA::getUnextendedValue(leafInst->getOperand(1))
                ->getType()
                ->getScalarSizeInBits() <= SILVIAMuladdOpSize))) {
        if (leafInst->getNumUses() > 1) {
          SmallVector<CallInst *, 4> pragmas;
          for (auto UI = leafInst->use_begin(), UE = leafInst->use_end();
               UI != UE; ++UI) {
            if (auto *user = dyn_cast<CallInst>(*UI)) {
              auto calleeName = user->getCalledFunction()->getName();
              // TODO: Do not clash with SpecFUCore pragma: if the third
              // parameter of _ssdm_op_SpecFUCore is 4, the mul should be
              // implemented in LUTs.
              if (calleeName == "_ssdm_op_SpecFUCore")
                pragmas.push_back(user);
            }
          }
          for (const auto &pragma : pragmas)
            pragma->eraseFromParent();
        }
        if (leafInst->hasOneUse())
          unpackedMuls[i].push_back(leafInst);
        else
          toAdd[i].push_back(leaf);
      } else {
        toAdd[i].push_back(leaf);
      }
    }
  }

  SmallVector<LeavesPack, 8> leavesPacks;
  SmallVector<int, 2> ext(instTuple.size());
  for (const auto &mulLeafA : unpackedMuls[0]) {
    auto packed = false;
    auto opA0 = mulLeafA->getOperand(0);
    auto opA1 = mulLeafA->getOperand(1);
    for (auto MI = unpackedMuls[1].begin(), ME = unpackedMuls[1].end();
         MI != ME; ++MI) {
      auto mulLeafB = *MI;
      auto opB0 = mulLeafB->getOperand(0);
      auto opB1 = mulLeafB->getOperand(1);

      if ((opA0 == opB0) || (opA0 == opB1) || (opA1 == opB0) ||
          (opA1 == opB1)) {
        if (leavesPacks.size() == 0) {
          ext[0] = SILVIA::getExtOpcode(mulLeafA);
          ext[1] = SILVIA::getExtOpcode(mulLeafB);
        } else if ((SILVIA::getExtOpcode(mulLeafA) != ext[0]) ||
                   (SILVIA::getExtOpcode(mulLeafB) != ext[1])) {
          return nullptr;
        }

        LeavesPack pack;
        pack.leaves.push_back(((opA0 != opB0) && (opA0 != opB1)) ? opA0 : opA1);
        pack.leaves.push_back(((opB0 != opA0) && (opB0 != opA1)) ? opB0 : opB1);
        pack.leaves.push_back(((opA0 == opB0) || (opA0 == opB1)) ? opA0 : opA1);
        pack.name = std::string(mulLeafA->getName()) + "_" +
                    std::string(mulLeafB->getName());

        leavesPacks.push_back(pack);
        packed = true;
        unpackedMuls[1].erase(MI);

        break;
      }
    }
    if (!packed)
      toAdd[0].push_back(mulLeafA);
  }

  for (const auto &mul : unpackedMuls[1])
    toAdd[1].push_back(mul);

  int maxChainLength = getMaxChainLength(leavesPacks, ext[1]);
  if ((SILVIAMuladdMaxChainLen > 0) &&
      (maxChainLength > SILVIAMuladdMaxChainLen))
    maxChainLength = SILVIAMuladdMaxChainLen;
  int numChains = std::ceil(leavesPacks.size() / ((float)maxChainLength));
  int balancedChainLength = std::ceil(leavesPacks.size() / ((float)numChains));
  auto ExtractProds =
      ((ext[1] == Instruction::SExt) ? ExtractProdsSign : ExtractProdsUnsign);
  Value *P = ConstantInt::get(IntegerType::get(context, 36), 0);
  SmallVector<Value *, 4> endsOfChain;
  SmallVector<Value *, 8> mulAddCalls;
  for (unsigned dspID = 0; dspID < leavesPacks.size(); ++dspID) {
    if ((dspID > 0) && ((dspID % balancedChainLength) == 0)) {
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
        args[i] = SILVIA::getUnextendedValue(args[i]);
        if (args[i]->getType()->getScalarSizeInBits() < argSize) {
          args[i] =
              builder.CreateTrunc(argOrig, IntegerType::get(context, argSize),
                                  argOrig->getName() + "_trunc");
        }
      }
      // This check evaluates to true when the precision of the mul operation
      // is actually less or equal to SILVIAMuladdOpSize bits.
      // The result is the same with both sext and zext since the higher bits
      // are ignored.
      if (args[i]->getType()->getScalarSizeInBits() < argSize) {
        args[i] =
            builder.CreateZExt(args[i], IntegerType::get(context, argSize),
                               args[i]->getName() + "_zext");
      }
    }
    P = builder.CreateCall(MulAdd, args, leavesPacks[dspID].name);
    mulAddCalls.push_back(P);
  }

  // 1. call extractProds from P
  endsOfChain.push_back(builder.CreateCall(ExtractProds, P));

  // 2. sum the extracted prods to the unpacked leafs
  for (unsigned i = 0; i < toAdd.size(); ++i) {
    for (const auto &endOfChain : endsOfChain)
      toAdd[i].push_back(builder.CreateExtractValue(endOfChain, i));
  }

  SmallVector<Value *, 2> sums;
  for (unsigned i = 0; i < toAdd.size(); ++i)
    sums.push_back(buildAddTree(toAdd[i], ext[i], builder, context));

  for (unsigned i = 0; i < sums.size(); ++i) {
    const auto rootSize =
        instTuple[i].outInst->getType()->getScalarSizeInBits();
    if (rootSize < sums[i]->getType()->getScalarSizeInBits()) {
      sums[i] =
          builder.CreateTrunc(sums[i], IntegerType::get(context, rootSize));
    } else if (rootSize > sums[i]->getType()->getScalarSizeInBits()) {
      sums[i] = ((ext[i] == Instruction::SExt)
                     ? builder.CreateSExt(sums[i],
                                          IntegerType::get(context, rootSize))
                     : builder.CreateZExt(sums[i],
                                          IntegerType::get(context, rootSize)));
    }
  }

  SmallVector<Type *, 2> rootTypes;
  for (const auto &root : sums)
    rootTypes.push_back(root->getType());
  auto rootStructTy = StructType::create(rootTypes);

  Value *rootStruct = UndefValue::get(rootStructTy);

  for (unsigned i = 0; i < sums.size(); ++i)
    rootStruct = builder.CreateInsertValue(rootStruct, sums[i], i);

  if (SILVIAMuladdInline) {
    InlineFunctionInfo IFI;
    for (const auto &P : mulAddCalls)
      InlineFunction(P, IFI);
    for (const auto &endOfChain : endsOfChain)
      InlineFunction(endOfChain, IFI);
  }

#ifdef DEBUG
  packedTrees += 2;
  packedLeaves += (2 * leavesPacks.size());
#endif /* DEBUG */

  return rootStruct;
}

bool SILVIAMuladd::runOnBasicBlock(BasicBlock &BB) {
  assert((SILVIAMuladdOpSize == 8) &&
         "SILVIAMuladd: unexpected value for SILVIAMuladdOpSize."
         "Possible values are: 8.");

  // Get the SIMD function
  Module *module = BB.getParent()->getParent();
  MulAdd = module->getFunction(
      "_simd_muladd" + std::string(SILVIAMuladdInline ? "_inline_" : "_") +
      "2");
  assert(MulAdd && "SIMD function not found");

  ExtractProdsSign = module->getFunction(
      "_simd_muladd_signed_extract" +
      std::string(SILVIAMuladdInline ? "_inline_" : "_") + "2");
  ExtractProdsUnsign = module->getFunction(
      "_simd_muladd_unsigned_extract" +
      std::string(SILVIAMuladdInline ? "_inline_" : "_") + "2");
  assert((ExtractProdsSign && ExtractProdsUnsign) &&
         "SIMD extract function not found");

  auto modified = SILVIA::runOnBasicBlock(BB);

  if (SILVIAMuladdInline && modified) {
    // InlineFunction may name some instructions with strings containing ".",
    // resulting in illegal RTL code: replace all "." with "_" characters in
    // the instruction names.
    for (auto &I : BB) {
      std::string name = I.getName().str();
      size_t pos = name.find('.');
      while (pos != std::string::npos) {
        name.replace(pos, 1, "_");
        pos = name.find('.', pos + 1);
      }
      I.setName(name);
    }
  }

  return modified;
}

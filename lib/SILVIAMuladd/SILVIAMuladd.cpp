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
    SILVIA::doInitialization(M);
    DEBUG(packedTrees = 0);
    DEBUG(packedLeaves = 0);
    return false;
  }

  std::list<SILVIA::Candidate> getCandidates(BasicBlock &BB) override;
  bool isCandidateCompatibleWithTuple(
      SILVIA::Candidate &candidate,
      SmallVector<SILVIA::Candidate, 4> &tuple) override;
  bool isTupleFull(SmallVector<SILVIA::Candidate, 4> &tuple) override;
  Value *packTuple(SmallVector<SILVIA::Candidate, 4> instTuple,
                   Instruction *insertBefore, LLVMContext &context) override;
  void printReport() override;

  Function *MulAddSign;
  Function *MulAddUnsign;
  Function *ExtractProdsSign;
  Function *ExtractProdsUnsign;
  unsigned long packedTrees;
  unsigned long packedLeaves;
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
    SILVIAMuladdMulOnly("silvia-muladd-mul-only", cl::init(false), cl::Hidden,
                        cl::desc("Whether to pack muladd or mul operations."));

static cl::opt<bool>
    SILVIAMuladdInline("silvia-muladd-inline", cl::init(false), cl::Hidden,
                       cl::desc("Whether to inline the packed operations."));

struct LeavesPack {
  SmallVector<Value *, 5> leaves;
  std::string name;
};

void getAddTree(Instruction *root, SILVIAMuladd::AddTree &tree) {
  if (root->getOpcode() != Instruction::Add) {
    tree.candidate.inVals.push_back(root);
    tree.candidate.outInst = root;
    return;
  }

  if (SILVIAMuladdMulOnly)
    return;

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
    const auto opcode = I->getOpcode();
    if ((opcode == Instruction::Add) || (opcode == Instruction::Mul)) {
      bool subset = false;
      if (opcode == Instruction::Add) {
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
      } else {
        for (const auto &tree : trees) {
          for (const auto &inVal : tree.candidate.inVals) {
            if (inVal == I) {
              subset = true;
              break;
            }
            if (subset)
              break;
          }
        }
      }

      if (subset)
        continue;

      SILVIAMuladd::AddTree tree;
      getAddTree(I, tree);

      auto validMuls = 0;
      auto muls = 0;
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
      if (validMuls > 0)
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

  for (const auto &candidateLeaf : candidate.inVals) {
    auto candidateLeafInst = dyn_cast<Instruction>(candidateLeaf);

    if ((!candidateLeafInst) ||
        (candidateLeafInst->getOpcode() != Instruction::Mul))
      continue;

    // TODO: Check if the dyn_casts do not return nullptr.
    auto candidateOp0 = SILVIA::getUnextendedValue(
        dyn_cast<Instruction>(candidateLeafInst->getOperand(0)));
    auto candidateOp1 = SILVIA::getUnextendedValue(
        dyn_cast<Instruction>(candidateLeafInst->getOperand(1)));
    if ((candidateOp0->getType()->getScalarSizeInBits() > SILVIAMuladdOpSize) ||
        (candidateOp1->getType()->getScalarSizeInBits() > SILVIAMuladdOpSize))
      continue;

    Value *commonOperand = nullptr;
    for (const auto &selected : tuple) {
      auto compatible = false;
      for (const auto &selectedLeaf : selected.inVals) {
        auto selectedLeafInst = dyn_cast<Instruction>(selectedLeaf);

        if ((!selectedLeafInst) ||
            (selectedLeafInst->getOpcode() != Instruction::Mul))
          continue;

        // TODO: Check if the dyn_casts do not return nullptr.
        auto selectedOp0 = SILVIA::getUnextendedValue(
            dyn_cast<Instruction>(selectedLeafInst->getOperand(0)));
        auto selectedOp1 = SILVIA::getUnextendedValue(
            dyn_cast<Instruction>(selectedLeafInst->getOperand(1)));

        if (commonOperand && (selectedOp0 != commonOperand) &&
            (selectedOp1 != commonOperand))
          continue;

        if ((selectedOp0->getType()->getScalarSizeInBits() >
             SILVIAMuladdOpSize) ||
            (selectedOp1->getType()->getScalarSizeInBits() >
             SILVIAMuladdOpSize))
          continue;

        if ((candidateOp0 == selectedOp0) || (candidateOp0 == selectedOp1)) {
          commonOperand = candidateOp0;
          compatible = true;
          break;
        }

        if ((candidateOp1 == selectedOp0) || (candidateOp1 == selectedOp1)) {
          commonOperand = candidateOp1;
          compatible = true;
          break;
        }
      }

      if (!compatible) {
        DEBUG(dbgs() << "SILVIAMuladd::isCandidateCompatibleWithTuple: "
                        "candidate rooted in "
                     << candidate.outInst->getName()
                     << " does not share an operand with candidate rooted in "
                     << tuple[0].outInst->getName() << ".\n");
        return false;
      }
    }
  }

  return true;
}

bool SILVIAMuladd::isTupleFull(SmallVector<SILVIA::Candidate, 4> &tuple) {
  return (tuple.size() == (16 / SILVIAMuladdOpSize));
}

unsigned getMaxChainLength(const SmallVector<LeavesPack, 8> &leavesPacks,
                           const int DExt) {
  if (SILVIAMuladdOpSize == 4)
    return 1;

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

  SmallVector<SmallVector<Value *, 8>, 4> toAdd(instTuple.size());
  SmallVector<std::list<Instruction *>, 4> unpackedMuls(instTuple.size());

  for (unsigned i = 0; i < instTuple.size(); ++i) {
    for (const auto &leaf : instTuple[i].inVals) {
      auto leafInst = dyn_cast<Instruction>(leaf);

      if (leafInst && (leafInst->getOpcode() == Instruction::Mul) &&
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
  SmallVector<int, 4> ext(instTuple.size());
  unsigned notCommonOpSign = 0;
  // Populate leavesPacks and toAdd with muls from unpackedMuls.
  for (unsigned i = 0, iEnd = unpackedMuls[0].size(); i < iEnd; ++i) {
    SmallVector<Instruction *, 4> compatibleLeaves((16 / SILVIAMuladdOpSize),
                                                   nullptr);
    Value *commonOperand = nullptr;
    compatibleLeaves[0] = unpackedMuls[0].front();
    unpackedMuls[0].pop_front();
    if (leavesPacks.size() == 0)
      ext[0] = SILVIA::getExtOpcode(compatibleLeaves[0]);
    for (unsigned j = 1, jEnd = unpackedMuls.size(); j < jEnd; ++j) {
      for (unsigned k = 0; k < unpackedMuls[j].size(); ++k) {
        auto mulLeaf = unpackedMuls[j].front();
        unpackedMuls[j].pop_front();
        Value *op[2] = {mulLeaf->getOperand(0), mulLeaf->getOperand(1)};
        if (!commonOperand) {
          Value *selOp[2] = {compatibleLeaves[0]->getOperand(0),
                             compatibleLeaves[0]->getOperand(1)};
          if ((op[0] == selOp[0]) || (op[0] == selOp[1])) {
            commonOperand = op[0];
          } else if ((op[1] == selOp[0]) || (op[1] == selOp[1])) {
            commonOperand = op[1];
          } else {
            unpackedMuls[j].push_back(mulLeaf);
            continue;
          }
          const auto notCommonSelOp =
              ((selOp[0] != commonOperand) ? selOp[0] : selOp[1]);
          if (const auto notCommonSelOpInst =
                  dyn_cast<Instruction>(notCommonSelOp))
            notCommonOpSign = notCommonSelOpInst->getOpcode();
        }
        if (commonOperand &&
            ((op[0] != commonOperand) && (op[1] != commonOperand))) {
          unpackedMuls[j].push_back(mulLeaf);
          continue;
        }

        const auto notCommonOp = ((op[0] != commonOperand) ? op[0] : op[1]);
        if (const auto notCommonOpInst = dyn_cast<Instruction>(notCommonOp)) {
          if ((SILVIAMuladdOpSize == 4) &&
              (notCommonOpInst->getOpcode() != notCommonOpSign)) {
            unpackedMuls[j].push_back(mulLeaf);
            continue;
          }
        }

        if (leavesPacks.size() == 0) {
          ext[j] = SILVIA::getExtOpcode(mulLeaf);
        } else if (SILVIA::getExtOpcode(mulLeaf) != ext[j]) {
          unpackedMuls[j].push_back(mulLeaf);
          continue;
        }

        compatibleLeaves[j] = mulLeaf;
        break;
      }
    }

    auto nCompatibleLeaves = 0;
    for (const auto &compatibleLeaf : compatibleLeaves)
      nCompatibleLeaves += (compatibleLeaf != nullptr);

    if (nCompatibleLeaves < 2) {
      unpackedMuls[0].push_back(compatibleLeaves[0]);
      continue;
    }

    LeavesPack pack;
    for (const auto &leaf : compatibleLeaves) {
      if (leaf) {
        Value *op[2] = {leaf->getOperand(0), leaf->getOperand(1)};
        pack.leaves.push_back((op[0] != commonOperand) ? op[0] : op[1]);
        pack.name +=
            ((pack.name == "") ? "" : "_") + std::string(leaf->getName());
        DEBUG(packedLeaves++);
      } else {
        pack.leaves.push_back(
            ConstantInt::get(IntegerType::get(context, SILVIAMuladdOpSize), 0));
      }
    }
    pack.leaves.push_back(commonOperand);

    leavesPacks.push_back(pack);
  }

  for (unsigned i = 0; i < unpackedMuls.size(); ++i) {
    for (const auto &mul : unpackedMuls[i])
      toAdd[i].push_back(mul);
  }
  if (leavesPacks.size() == 0)
    return nullptr;

  int maxChainLength = getMaxChainLength(leavesPacks, ext[1]);
  if ((SILVIAMuladdMaxChainLen > 0) &&
      (maxChainLength > SILVIAMuladdMaxChainLen))
    maxChainLength = SILVIAMuladdMaxChainLen;
  int numChains = std::ceil(leavesPacks.size() / ((float)maxChainLength));
  int balancedChainLength = std::ceil(leavesPacks.size() / ((float)numChains));
  auto MulAdd =
      ((notCommonOpSign == Instruction::SExt) ? MulAddSign : MulAddUnsign);
  auto ExtractProds = (((ext[1] != Instruction::SExt) ||
                        ((SILVIAMuladdOpSize == 4) && (MulAdd == MulAddSign)))
                           ? ExtractProdsUnsign
                           : ExtractProdsSign);

  const unsigned packedProdSize = ((SILVIAMuladdOpSize == 8) ? 36 : 32);
  Value *P = ConstantInt::get(IntegerType::get(context, packedProdSize), 0);
  SmallVector<Value *, 4> endsOfChain;
  SmallVector<Value *, 8> mulAddCalls;
  for (unsigned dspID = 0; dspID < leavesPacks.size(); ++dspID) {
    if ((dspID > 0) && ((dspID % balancedChainLength) == 0)) {
      endsOfChain.push_back(builder.CreateCall(ExtractProds, P));
      P = ConstantInt::get(IntegerType::get(context, packedProdSize), 0);
    }
    // pack mulLeafA and mulLeafB + sum P
    // assign the result to P
    SmallVector<Value *, 6> args;
    for (const auto &leaf : leavesPacks[dspID].leaves)
      args.push_back(leaf);
    if (SILVIAMuladdOpSize == 8)
      args.push_back(P);
    auto MulAddTy = cast<FunctionType>(
        cast<PointerType>(MulAdd->getType())->getElementType());
    for (unsigned i = 0; i < leavesPacks[dspID].leaves.size(); ++i) {
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
    if ((SILVIAMuladdOpSize == 4) && (MulAdd == MulAddUnsign) &&
        (ExtractProds == ExtractProdsUnsign))
      P = builder.CreateExtractValue(P, 0);
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

  InlineFunctionInfo IFI;
  if (SILVIAMuladdInline ||
      ((SILVIAMuladdOpSize == 4) && (MulAdd == MulAddUnsign))) {
    for (const auto &P : mulAddCalls)
      InlineFunction(P, IFI);
  }
  if (SILVIAMuladdInline || (SILVIAMuladdOpSize == 4)) {
    for (const auto &endOfChain : endsOfChain)
      InlineFunction(endOfChain, IFI);
  }

  DEBUG(packedTrees += instTuple.size());

  return rootStruct;
}

void SILVIAMuladd::printReport() {
  if (!SILVIAMuladdMulOnly) {
    DEBUG(dbgs() << "SILVIAMuladd::printReport: packed " << packedTrees
                 << " trees (" << packedLeaves << " leaves).\n");
  }
  SILVIA::printReport();
}

bool SILVIAMuladd::runOnBasicBlock(BasicBlock &BB) {
  assert(((SILVIAMuladdOpSize == 8) || (SILVIAMuladdOpSize == 4)) &&
         "SILVIAMuladd: unexpected value for SILVIAMuladdOpSize."
         "Possible values are: 4, 8.");

  // Get the SIMD function
  Module *module = BB.getParent()->getParent();

  MulAddSign = module->getFunction(
      "_silvia_muladd" + std::string((SILVIAMuladdOpSize == 4) ? "_signed" : "") +
      std::string(((SILVIAMuladdOpSize == 8) && SILVIAMuladdInline) ? "_inline_"
                                                                    : "_") +
      std::to_string(SILVIAMuladdOpSize) + "b");
  MulAddUnsign =
      ((SILVIAMuladdOpSize == 8)
           ? MulAddSign
           : module->getFunction("_silvia_muladd_unsigned_" +
                                 std::to_string(SILVIAMuladdOpSize) + "b"));
  assert((MulAddSign && MulAddUnsign) && "SIMD function not found");

  ExtractProdsSign = module->getFunction(
      "_silvia_muladd_signed_extract" +
      std::string(((SILVIAMuladdOpSize == 8) && SILVIAMuladdInline) ? "_inline_"
                                                                    : "_") +
      std::to_string(SILVIAMuladdOpSize) + "b");
  ExtractProdsUnsign = module->getFunction(
      "_silvia_muladd_unsigned_extract" +
      std::string(((SILVIAMuladdOpSize == 8) && SILVIAMuladdInline) ? "_inline_"
                                                                    : "_") +
      std::to_string(SILVIAMuladdOpSize) + "b");

  assert((ExtractProdsSign && ExtractProdsUnsign) &&
         "SIMD extract function not found");

  auto modified = SILVIA::runOnBasicBlock(BB);

  if ((SILVIAMuladdInline || (SILVIAMuladdOpSize == 4)) && modified) {
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

#include "llvm/ADT/SmallVector.h"
#include "llvm/BasicBlock.h"
#include "llvm/Function.h"
#include "llvm/Module.h"
#include "llvm/Pass.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/IRBuilder.h"
#include "llvm/Transforms/Utils/Cloning.h"

#include <cassert>
#include <list>

#include "../SILVIA/SILVIA.h"

using namespace llvm;

struct SILVIAMul : public SILVIA {
  static char ID;
  SILVIAMul() : SILVIA(ID) {}

  bool runOnBasicBlock(BasicBlock &BB) override;

  std::list<SILVIA::Candidate> getSIMDableInstructions(BasicBlock &BB) override;
  bool isCandidateCompatibleWithTuple(
      SILVIA::Candidate &candidate,
      SmallVector<SILVIA::Candidate, 4> &tuple) override;
  virtual bool isTupleFull(SmallVector<SILVIA::Candidate, 4> &tuple);
  void replaceInstsWithSIMDCall(SmallVector<SILVIA::Candidate, 4> instTuple,
                                Instruction *insertBefore,
                                LLVMContext &context) override;

  Function *MulAdd;
  Function *ExtractProdsSign;
  Function *ExtractProdsUnsign;
};

char SILVIAMul::ID = 0;
static RegisterPass<SILVIAMul> X("silvia-mul", "Pack muls to SIMD DSPs",
                                 false /* Only looks at CFG */,
                                 true /* Transformation Pass */);

// Collect all the add instructions.
std::list<SILVIA::Candidate>
SILVIAMul::getSIMDableInstructions(BasicBlock &BB) {
  std::list<SILVIA::Candidate> candidateInsts;

  const auto mulMaxWidth = 8;

  for (auto &I : BB) {
    if (I.getOpcode() != Instruction::Mul)
      continue;

    SILVIA::Candidate candidate;
    candidate.outInst = &I;
    for (unsigned i = 0; i < I.getNumOperands(); ++i) {
      auto op = SILVIA::getUnextendedValue(I.getOperand(i));
      if (op->getType()->getScalarSizeInBits() <= mulMaxWidth)
        candidate.inVals.push_back(I.getOperand(i));
    }

    if (candidate.inVals.size() == I.getNumOperands())
      candidateInsts.push_back(candidate);
  }

  return candidateInsts;
}

bool SILVIAMul::isCandidateCompatibleWithTuple(
    SILVIA::Candidate &candidate, SmallVector<SILVIA::Candidate, 4> &tuple) {
  if (tuple.size() == 0)
    return true;
  auto opA0 = candidate.inVals[0];
  auto opA1 = candidate.inVals[1];
  auto opB0 = tuple[0].inVals[0];
  auto opB1 = tuple[0].inVals[1];

  return ((opA0 == opB0) || (opA0 == opB1) || (opA1 == opB0) || (opA1 == opB1));
}

bool SILVIAMul::isTupleFull(SmallVector<SILVIA::Candidate, 4> &tuple) {
  return (tuple.size() == 2);
}

void SILVIAMul::replaceInstsWithSIMDCall(
    SmallVector<SILVIA::Candidate, 4> instTuple, Instruction *insertBefore,
    LLVMContext &context) {
  IRBuilder<> builder(insertBefore);

  SmallVector<Value *, 3> pack;
  std::string packName;
  SmallVector<int, 2> ext(instTuple.size());
  ext[0] = SILVIA::getExtOpcode(instTuple[0].outInst);
  ext[1] = SILVIA::getExtOpcode(instTuple[1].outInst);

  auto ExtractProds =
      ((ext[1] == Instruction::SExt) ? ExtractProdsSign : ExtractProdsUnsign);

  pack.push_back(((instTuple[0].inVals[0] != instTuple[1].inVals[0]) &&
                  (instTuple[0].inVals[0] != instTuple[1].inVals[1]))
                     ? instTuple[0].inVals[0]
                     : instTuple[0].inVals[1]);
  pack.push_back(((instTuple[1].inVals[0] != instTuple[0].inVals[0]) &&
                  (instTuple[1].inVals[0] != instTuple[0].inVals[1]))
                     ? instTuple[1].inVals[0]
                     : instTuple[1].inVals[1]);
  pack.push_back(((instTuple[0].inVals[0] == instTuple[1].inVals[0]) ||
                  (instTuple[0].inVals[0] == instTuple[1].inVals[1]))
                     ? instTuple[0].inVals[0]
                     : instTuple[0].inVals[1]);
  packName = std::string(instTuple[0].outInst->getName()) + "_" +
             std::string(instTuple[1].outInst->getName());

  // pack mulLeafA and mulLeafB
  // assign the result to P
  Value *args[3] = {pack[0], pack[1], pack[2]};
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
      args[i] = builder.CreateZExt(args[i], IntegerType::get(context, argSize),
                                   args[i]->getName() + "_zext");
    }
  }
  auto P = builder.CreateCall(MulAdd, args, packName);

  // 1. call extractProds from P
  auto endOfChain = builder.CreateCall(ExtractProds, P);

  // 2. sum the extracted prods to the unpacked leafs
  SmallVector<Value *, 2> prods;
  for (unsigned i = 0; i < instTuple.size(); ++i)
    prods.push_back(builder.CreateExtractValue(endOfChain, i));

  for (unsigned i = 0; i < prods.size(); ++i) {
    const auto origSize =
        instTuple[i].outInst->getType()->getScalarSizeInBits();
    if (origSize < prods[i]->getType()->getScalarSizeInBits()) {
      prods[i] =
          builder.CreateTrunc(prods[i], IntegerType::get(context, origSize));
    } else if (origSize > prods[i]->getType()->getScalarSizeInBits()) {
      prods[i] = ((ext[i] == Instruction::SExt)
                      ? builder.CreateSExt(prods[i],
                                           IntegerType::get(context, origSize))
                      : builder.CreateZExt(
                            prods[i], IntegerType::get(context, origSize)));
    }
  }

  // 3. replaceAllUsesWith prods
  SmallVector<std::string, 2> prodNames;
  for (unsigned i = 0; i < instTuple.size(); ++i) {
    instTuple[i].outInst->replaceAllUsesWith(prods[i]);
    prodNames.push_back(instTuple[i].outInst->getName());
  }

  for (auto prod : instTuple)
    prod.outInst->eraseFromParent();

  for (unsigned i = 0; i < prods.size(); ++i)
    prods[i]->setName(prodNames[i]);

  //  InlineFunctionInfo IFI;
  //  InlineFunction(P, IFI);
  //  InlineFunction(endOfChain, IFI);
}

bool SILVIAMul::runOnBasicBlock(BasicBlock &BB) {
  // Get the SIMD function
  Module *module = BB.getParent()->getParent();
  MulAdd = module->getFunction("_simd_mul_2");
  assert(MulAdd && "SIMD function not found");

  ExtractProdsSign = module->getFunction("_simd_mul_signed_extract_2");
  ExtractProdsUnsign = module->getFunction("_simd_mul_unsigned_extract_2");
  assert((ExtractProdsSign && ExtractProdsUnsign) &&
         "SIMD extract function not found");

  return SILVIA::runOnBasicBlock(BB);
}

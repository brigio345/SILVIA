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

  std::list<SILVIA::Candidate> getCandidates(BasicBlock &BB) override;
  bool isCandidateCompatibleWithTuple(
      SILVIA::Candidate &candidate,
      SmallVector<SILVIA::Candidate, 4> &tuple) override;
  virtual bool isTupleFull(SmallVector<SILVIA::Candidate, 4> &tuple);
  Value *packTuple(SmallVector<SILVIA::Candidate, 4> instTuple,
                   Instruction *insertBefore, LLVMContext &context) override;

  Function *MulAddSign;
  Function *MulAddUnsign;
};

char SILVIAMul::ID = 0;
static RegisterPass<SILVIAMul> X("silvia-mul", "Pack muls to SIMD DSPs",
                                 false /* Only looks at CFG */,
                                 true /* Transformation Pass */);

static cl::opt<bool>
    SILVIAMulInline("silvia-mul-inline", cl::init(false), cl::Hidden,
                    cl::desc("Whether to inline the packed operations."));

// Collect all the add instructions.
std::list<SILVIA::Candidate> SILVIAMul::getCandidates(BasicBlock &BB) {
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

Value *SILVIAMul::packTuple(SmallVector<SILVIA::Candidate, 4> instTuple,
                            Instruction *insertBefore, LLVMContext &context) {
  IRBuilder<> builder(insertBefore);

  SmallVector<Value *, 3> pack;
  std::string packName;
  SmallVector<int, 2> ext(instTuple.size());
  ext[0] = SILVIA::getExtOpcode(instTuple[0].outInst);
  ext[1] = SILVIA::getExtOpcode(instTuple[1].outInst);

  auto MulAdd = ((ext[1] == Instruction::SExt) ? MulAddSign : MulAddUnsign);

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

  // 1. sum the extracted prods to the unpacked leafs
  SmallVector<Value *, 2> prods;
  for (unsigned i = 0; i < instTuple.size(); ++i)
    prods.push_back(builder.CreateExtractValue(P, i));

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

  SmallVector<Type *, 2> prodTypes;
  for (auto prod : prods)
    prodTypes.push_back(prod->getType());
  auto prodStructTy = StructType::create(prodTypes);

  Value *prodStruct = UndefValue::get(prodStructTy);

  for (unsigned i = 0; i < prods.size(); ++i)
    prodStruct = builder.CreateInsertValue(prodStruct, prods[i], i);

  if (SILVIAMulInline) {
    InlineFunctionInfo IFI;
    InlineFunction(P, IFI);
  }

  return prodStruct;
}

bool SILVIAMul::runOnBasicBlock(BasicBlock &BB) {
  // Get the SIMD function
  Module *module = BB.getParent()->getParent();
  MulAddSign = module->getFunction(
      "_simd_mul_signed" + std::string(SILVIAMulInline ? "_inline_" : "_") +
      "2");
  MulAddUnsign = module->getFunction(
      "_simd_mul_unsigned" + std::string(SILVIAMulInline ? "_inline_" : "_") +
      "2");
  assert((MulAddSign && MulAddUnsign) && "SIMD functions not found");

  auto modified = SILVIA::runOnBasicBlock(BB);

  if (SILVIAMulInline && modified) {
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

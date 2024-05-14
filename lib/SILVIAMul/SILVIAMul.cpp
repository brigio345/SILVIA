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

#include "SILVIA/SILVIA.h"

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

static cl::opt<unsigned int>
    SILVIAMulSIMDFactor("silvia-mul-simd-factor", cl::init(2), cl::Hidden,
                        cl::desc("The amount of muls to pack to SIMD DSPs."));

// Collect all the add instructions.
std::list<SILVIA::Candidate> SILVIAMul::getCandidates(BasicBlock &BB) {
  std::list<SILVIA::Candidate> candidateInsts;

  const auto mulMaxWidth = (16 / SILVIAMulSIMDFactor);

  for (auto &I : BB) {
    if (I.getOpcode() != Instruction::Mul)
      continue;

    SILVIA::Candidate candidate;
    candidate.outInst = &I;
    for (unsigned i = 0; i < I.getNumOperands(); ++i) {
      auto op = SILVIA::getUnextendedValue(I.getOperand(i));
      if (isa<Constant>(op))
        break;
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
  Value *commonOperand = nullptr;
  for (auto selected : tuple) {
    if (commonOperand && ((opA0 != commonOperand) && (opA1 != commonOperand)))
      return false;

    Value *notCommonOperand;
    if ((opA0 == selected.inVals[0]) || (opA0 == selected.inVals[1])) {
      commonOperand = opA0;
      notCommonOperand = opA1;
    } else if ((opA1 == selected.inVals[0]) || (opA1 == selected.inVals[1])) {
      commonOperand = opA1;
      notCommonOperand = opA0;
    } else {
      return false;
    }

    if (SILVIAMulSIMDFactor == 4) {
      if (auto notCommonOperandInst = dyn_cast<Instruction>(
              SILVIA::getUnextendedValue(notCommonOperand))) {
        if (SILVIA::getExtOpcode(notCommonOperandInst) == Instruction::ZExt) {
          DEBUG(dbgs() << "SILVIAMul::isCandidateCompatibleWithTuple: cannot "
                          "pack a tuple with a "
                          "non-shared unsigned operand.\n");
          return false;
        }
      }
    }
  }

  return true;
}

bool SILVIAMul::isTupleFull(SmallVector<SILVIA::Candidate, 4> &tuple) {
  return (tuple.size() == SILVIAMulSIMDFactor);
}

Value *SILVIAMul::packTuple(SmallVector<SILVIA::Candidate, 4> instTuple,
                            Instruction *insertBefore, LLVMContext &context) {
  IRBuilder<> builder(insertBefore);

  SmallVector<int, 4> ext(instTuple.size());
  for (unsigned i = 0; i < instTuple.size(); ++i)
    ext[i] = SILVIA::getExtOpcode(instTuple[i].outInst);

  auto MulAdd = (((SILVIAMulSIMDFactor == 4) || (ext[1] == Instruction::SExt))
                     ? MulAddSign
                     : MulAddUnsign);

  auto commonOperand = (((instTuple[0].inVals[0] == instTuple[1].inVals[0]) ||
                         (instTuple[0].inVals[0] == instTuple[1].inVals[1]))
                            ? instTuple[0].inVals[0]
                            : instTuple[0].inVals[1]);

  SmallVector<Value *, 5> args(
      SILVIAMulSIMDFactor + 1,
      ConstantInt::get(IntegerType::get(context, (16 / SILVIAMulSIMDFactor)),
                       0));
  std::string packName = "";
  for (unsigned i = 0; i < instTuple.size(); ++i) {
    args[i] =
        (((instTuple[i].inVals[0] != commonOperand)) ? instTuple[i].inVals[0]
                                                     : instTuple[i].inVals[1]);
    packName = (((packName == "") ? "" : "_") +
                std::string(instTuple[i].outInst->getName()));
  }
  args[SILVIAMulSIMDFactor] = commonOperand;

  // pack mulLeafA and mulLeafB
  // assign the result to P
  auto MulAddTy = cast<FunctionType>(
      cast<PointerType>(MulAdd->getType())->getElementType());
  for (unsigned i = 0; i < args.size(); ++i) {
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
  SmallVector<Value *, 4> prods;
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

  SmallVector<Type *, 4> prodTypes;
  for (auto prod : prods)
    prodTypes.push_back(prod->getType());
  auto prodStructTy = StructType::create(prodTypes);

  Value *prodStruct = UndefValue::get(prodStructTy);

  for (unsigned i = 0; i < prods.size(); ++i)
    prodStruct = builder.CreateInsertValue(prodStruct, prods[i], i);

  if ((SILVIAMulSIMDFactor != 4) && SILVIAMulInline) {
    InlineFunctionInfo IFI;
    InlineFunction(P, IFI);
  }

  return prodStruct;
}

bool SILVIAMul::runOnBasicBlock(BasicBlock &BB) {
  // Get the SIMD function
  Module *module = BB.getParent()->getParent();
  if (SILVIAMulSIMDFactor != 4) {
    MulAddSign = module->getFunction(
        "_simd_mul_signed" + std::string(SILVIAMulInline ? "_inline_" : "_") +
        std::to_string(SILVIAMulSIMDFactor));
    MulAddUnsign = module->getFunction(
        "_simd_mul_unsigned" + std::string(SILVIAMulInline ? "_inline_" : "_") +
        std::to_string(SILVIAMulSIMDFactor));
    assert((MulAddSign && MulAddUnsign) && "SIMD functions not found");
  } else {
    MulAddSign =
        module->getFunction("_simd_mul_" + std::to_string(SILVIAMulSIMDFactor));
    MulAddUnsign = nullptr;
    assert((MulAddSign) && "SIMD functions not found");
  }

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

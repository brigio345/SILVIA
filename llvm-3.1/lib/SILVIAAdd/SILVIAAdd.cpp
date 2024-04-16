#include "llvm/ADT/SmallVector.h"
#include "llvm/BasicBlock.h"
#include "llvm/Function.h"
#include "llvm/Module.h"
#include "llvm/Pass.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/IRBuilder.h"

#include <cassert>
#include <list>

#include "../SILVIA/SILVIA.h"

using namespace llvm;

struct SILVIAAdd : public SILVIA {
  static char ID;
  SILVIAAdd() : SILVIA(ID) {}

  bool runOnBasicBlock(BasicBlock &BB) override;

  std::list<SILVIA::Candidate> getSIMDableInstructions(BasicBlock &BB) override;
  bool isCandidateCompatibleWithTuple(
      SILVIA::Candidate &candidate,
      SmallVector<SILVIA::Candidate, 4> &tuple) override;
  virtual bool isTupleFull(SmallVector<SILVIA::Candidate, 4> &tuple);
  void replaceInstsWithSIMDCall(SmallVector<SILVIA::Candidate, 4> instTuple,
                                Instruction *insertBefore,
                                LLVMContext &context) override;

  Function *SIMDFunc;
};

char SILVIAAdd::ID = 0;
static RegisterPass<SILVIAAdd> X("silvia-add", "Pack adds to SIMD DSPs",
                                 false /* Only looks at CFG */,
                                 true /* Transformation Pass */);

static cl::opt<unsigned int>
    SIMDFactor("silvia-add-simd-factor", cl::init(4), cl::Hidden,
               cl::desc("The amount of adds to pack to SIMD DSPs."));

static cl::opt<unsigned int> DSPWidth("silvia-add-dsp-width", cl::init(48),
                                      cl::Hidden,
                                      cl::desc("The DSP width in bits."));

// Collect all the add instructions.
std::list<SILVIA::Candidate>
SILVIAAdd::getSIMDableInstructions(BasicBlock &BB) {
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

bool SILVIAAdd::isCandidateCompatibleWithTuple(
    SILVIA::Candidate &candidate, SmallVector<SILVIA::Candidate, 4> &tuple) {
  return true;
}

bool SILVIAAdd::isTupleFull(SmallVector<SILVIA::Candidate, 4> &tuple) {
  return (tuple.size() == SIMDFactor);
}

void SILVIAAdd::replaceInstsWithSIMDCall(
    SmallVector<SILVIA::Candidate, 4> instTuple, Instruction *insertBefore,
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

bool SILVIAAdd::runOnBasicBlock(BasicBlock &BB) {
  assert(((SIMDFactor == 2) || (SIMDFactor == 4)) &&
         "Unexpected value for silvia-simd-factor option.");
  assert((DSPWidth == 48) && "Unexpected value for silvia-dsp-width option.");

  // Get the SIMD function
  Module *module = BB.getParent()->getParent();
  SIMDFunc = module->getFunction("_simd_add_" + std::to_string(SIMDFactor));
  assert(SIMDFunc && "SIMD function not found");

  return SILVIA::runOnBasicBlock(BB);
}

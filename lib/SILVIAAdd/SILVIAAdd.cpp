#include "llvm/ADT/SmallVector.h"
#include "llvm/BasicBlock.h"
#include "llvm/Function.h"
#include "llvm/Module.h"
#include "llvm/Pass.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/IRBuilder.h"

#include <cassert>
#include <list>

#include "SILVIA/SILVIA.h"

using namespace llvm;

struct SILVIAAdd : public SILVIA {
  static char ID;
  SILVIAAdd() : SILVIA(ID) {}

  bool runOnBasicBlock(BasicBlock &BB) override;

  std::list<SILVIA::Candidate> getCandidates(BasicBlock &BB) override;
  bool isCandidateCompatibleWithTuple(
      SILVIA::Candidate &candidate,
      SmallVector<SILVIA::Candidate, 4> &tuple) override;
  virtual bool isTupleFull(SmallVector<SILVIA::Candidate, 4> &tuple);
  Value *packTuple(SmallVector<SILVIA::Candidate, 4> instTuple,
                   Instruction *insertBefore, LLVMContext &context) override;

  Function *SIMDFunc;
};

char SILVIAAdd::ID = 0;
static RegisterPass<SILVIAAdd> X("silvia-add", "Pack adds to SIMD DSPs",
                                 false /* Only looks at CFG */,
                                 true /* Transformation Pass */);

static cl::opt<unsigned int>
    SIMDFactor("silvia-add-simd-factor", cl::init(4), cl::Hidden,
               cl::desc("The amount of adds to pack to SIMD DSPs."));

static cl::opt<std::string> SIMDOp(
    "silvia-add-op", cl::init("add"), cl::Hidden,
    cl::desc(
        "The operation to pack to SIMD DSPs. Valid values are: add, sub."));

static cl::opt<unsigned int> DSPWidth("silvia-add-dsp-width", cl::init(48),
                                      cl::Hidden,
                                      cl::desc("The DSP width in bits."));

// Collect all the add instructions.
std::list<SILVIA::Candidate> SILVIAAdd::getCandidates(BasicBlock &BB) {
  std::list<SILVIA::Candidate> candidateInsts;

  const auto addMaxWidth = (DSPWidth / SIMDFactor);

  auto instTy = ((SIMDOp == "add") ? Instruction::Add : Instruction::Sub);
  for (auto &I : BB) {
    if (I.getOpcode() != instTy)
      continue;
    if (isa<Constant>(I.getOperand(0)) || isa<Constant>(I.getOperand(1)))
      continue;
    if (I.getType()->getScalarSizeInBits() <= addMaxWidth) {
      SILVIA::Candidate candidate;
      candidate.inVals.push_back(&I);
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

Value *SILVIAAdd::packTuple(SmallVector<SILVIA::Candidate, 4> instTuple,
                            Instruction *insertBefore, LLVMContext &context) {
  IRBuilder<> builder(insertBefore);

  const auto dataBitWidth = (DSPWidth / SIMDFactor);

  SmallVector<Value *, 8> args(
      SIMDFunc->getArgumentList().size(),
      ConstantInt::get(IntegerType::get(context, dataBitWidth), 0));
  std::string retName = "";
  for (unsigned i = 0; i < instTuple.size(); ++i) {
    retName = retName + ((retName == "") ? "" : "_") +
              instTuple[i].outInst->getName().str();
    auto addInst = cast<Instruction>(instTuple[i].inVals[0]);
    for (unsigned j = 0; j < addInst->getNumOperands(); ++j) {
      auto operand = addInst->getOperand(j);

      auto arg = ((operand->getType()->getScalarSizeInBits() < dataBitWidth)
                      ? builder.CreateZExt(
                            operand, IntegerType::get(context, dataBitWidth),
                            operand->getName() + "_zext")
                      : operand);
      args[i * addInst->getNumOperands() + j] = arg;
    }
  }

  Value *sumAggr = builder.CreateCall(SIMDFunc, args, retName);

  SmallVector<Value *, 4> adds(instTuple.size());
  for (unsigned i = 0; i < instTuple.size(); ++i) {
    adds[i] = builder.CreateExtractValue(
        sumAggr, i, instTuple[i].outInst->getName() + "_zext");
    if (instTuple[i].outInst->getType()->getScalarSizeInBits() < dataBitWidth)
      adds[i] = builder.CreateTrunc(adds[i], instTuple[i].outInst->getType());
  }

  // Replace the add instruction with the result
  SmallVector<Type *, 2> addTypes;
  for (auto add : adds)
    addTypes.push_back(add->getType());
  auto addStructTy = StructType::create(addTypes);

  Value *addStruct = UndefValue::get(addStructTy);

  for (unsigned i = 0; i < adds.size(); ++i)
    addStruct = builder.CreateInsertValue(addStruct, adds[i], i);

  return addStruct;
}

bool SILVIAAdd::runOnBasicBlock(BasicBlock &BB) {
  assert(((SIMDFactor == 2) || (SIMDFactor == 4)) &&
         "Unexpected value for silvia-simd-factor option.");
  assert((DSPWidth == 48) && "Unexpected value for silvia-dsp-width option.");

  // Get the SIMD function
  Module *module = BB.getParent()->getParent();
  dbgs() << "searcing for "
         << "_simd_" + SIMDOp + "_" + std::to_string(SIMDFactor) << "\n";
  SIMDFunc =
      module->getFunction("_simd_" + SIMDOp + "_" + std::to_string(SIMDFactor));
  assert(SIMDFunc && "SIMD function not found");

  return SILVIA::runOnBasicBlock(BB);
}

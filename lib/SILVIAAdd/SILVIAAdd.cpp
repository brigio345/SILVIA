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
  SILVIAAdd() : SILVIA(ID, true) {}

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
    SILVIAAddOpSize("silvia-add-op-size", cl::init(12), cl::Hidden,
                    cl::desc("The maximum size in bits of the addends."));

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

  auto instTy = ((SIMDOp == "add") ? Instruction::Add : Instruction::Sub);
  for (auto &I : BB) {
    if (I.getOpcode() != instTy)
      continue;
    if (isa<Constant>(I.getOperand(0)) || isa<Constant>(I.getOperand(1)))
      continue;
    if (I.getType()->getScalarSizeInBits() <= SILVIAAddOpSize) {
      SILVIA::Candidate candidate;
      for (unsigned i = 0; i < I.getNumOperands(); ++i)
        candidate.inVals.push_back(I.getOperand(i));
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
  return (tuple.size() == (DSPWidth / SILVIAAddOpSize));
}

Value *SILVIAAdd::packTuple(SmallVector<SILVIA::Candidate, 4> instTuple,
                            Instruction *insertBefore, LLVMContext &context) {
  IRBuilder<> builder(insertBefore);

  SmallVector<Value *, 8> args(
      SIMDFunc->getArgumentList().size(),
      ConstantInt::get(IntegerType::get(context, SILVIAAddOpSize), 0));
  std::string retName = "";
  for (unsigned i = 0; i < instTuple.size(); ++i) {
    retName = retName + ((retName == "") ? "" : "_") +
              instTuple[i].outInst->getName().str();
    auto addInst = instTuple[i].outInst;
    for (unsigned j = 0; j < addInst->getNumOperands(); ++j) {
      auto operand = addInst->getOperand(j);

      auto arg = ((operand->getType()->getScalarSizeInBits() < SILVIAAddOpSize)
                      ? builder.CreateZExt(
                            operand, IntegerType::get(context, SILVIAAddOpSize),
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
    if (instTuple[i].outInst->getType()->getScalarSizeInBits() <
        SILVIAAddOpSize)
      adds[i] = builder.CreateTrunc(adds[i], instTuple[i].outInst->getType());
  }

  // Replace the add instruction with the result
  SmallVector<Type *, 2> addTypes;
  for (const auto &add : adds)
    addTypes.push_back(add->getType());
  auto addStructTy = StructType::create(addTypes);

  Value *addStruct = UndefValue::get(addStructTy);

  for (unsigned i = 0; i < adds.size(); ++i)
    addStruct = builder.CreateInsertValue(addStruct, adds[i], i);

  return addStruct;
}

bool SILVIAAdd::runOnBasicBlock(BasicBlock &BB) {
  assert(((SILVIAAddOpSize == 12) || (SILVIAAddOpSize == 24)) &&
         "Unexpected value for silvia-op-size option."
         "Possible values are: 12, 24.");
  assert((DSPWidth == 48) && "Unexpected value for silvia-dsp-width option.");

  // Get the SIMD function
  Module *module = BB.getParent()->getParent();
  SIMDFunc = module->getFunction("_silvia_" + SIMDOp + "_" +
                                 std::to_string(SILVIAAddOpSize) + "b");
  assert(SIMDFunc && "SIMD function not found");

  return SILVIA::runOnBasicBlock(BB);
}

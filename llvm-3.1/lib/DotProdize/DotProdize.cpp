#include "llvm/ADT/SmallVector.h"
#include "llvm/BasicBlock.h"
#include "llvm/Function.h"
#include "llvm/Instructions.h"
#include "llvm/Module.h"
#include "llvm/Pass.h"
#include "llvm/Support/IRBuilder.h"

#include <cassert>
#include <list>

using namespace llvm;

struct DotProdize : public BasicBlockPass {
  static char ID;
  DotProdize() : BasicBlockPass(ID) {}

  bool runOnBasicBlock(BasicBlock &BB) override;
};

struct CandidateInst {
  SmallVector<Value *, 6> inputs;
  SmallVector<Value *, 1> outputs;
};

char DotProdize::ID = 0;
static RegisterPass<DotProdize> X("dot-prod-ize",
                                  "Dot products to DSP58 dot product mode.",
                                  false /* Only looks at CFG */,
                                  true /* Transformation Pass */);

bool getDotProdTree(Instruction *addRoot,
                    SmallVector<Instruction *, 8> &mulLeafs,
                    SmallVector<Instruction *, 8> &addInternal) {
  if (addRoot->getOpcode() != Instruction::Add)
    return false;

  for (unsigned i = 0; i < addRoot->getNumOperands(); ++i) {
    auto op = dyn_cast<Instruction>(addRoot->getOperand(i));
    if (!op)
      return false;

    if (op->getOpcode() == Instruction::SExt)
      op = dyn_cast<Instruction>(op->getOperand(0));
    if (!op)
      return false;

    // The tree cannot absorb an op with multiple uses, since its value is
    // needed elsewhere too.
    if (!op->hasOneUse())
      return false;

    switch (op->getOpcode()) {
    case Instruction::Mul:
      mulLeafs.push_back(op);
      break;
    case Instruction::Add:
      if (!getDotProdTree(op, mulLeafs, addInternal))
        return false;
      addInternal.push_back(op);
      break;
    default:
      return false;
    }
  }

  return true;
}

// Collect all the add instructions.
void getCandidates(BasicBlock &BB, std::list<CandidateInst> &candidateInsts) {
  // TODO: manage constant operands.
  for (auto &I : BB) {
    // \ 8   / 8 \ 8   / 8 \ 8   / 8
    //  \   /     \   /     \   /
    //  (m_0)     (m_1)     (m_2)
    //    \         /         /
    //     \       /         /
    //      \     /         /
    //       \   /         /
    //       (a_0)        /
    //         \         /
    //          \       /
    //           \     /
    //            \   /
    //            (a_1)
    //              |
    //              |

    CandidateInst candidate;

    // Search for a_1.
    if (I.getOpcode() != Instruction::Add)
      continue;

    candidate.outputs.push_back(&I);

    // Search for m_2.
    auto m2Branch = -1;

    for (unsigned i = 0; i < I.getNumOperands(); ++i) {
      auto opInst = dyn_cast<Instruction>(I.getOperand(i));
      if (!opInst)
        continue;

      if (opInst->getOpcode() == Instruction::SExt)
        opInst = dyn_cast<Instruction>(opInst->getOperand(0));

      if (opInst->getOpcode() == Instruction::Mul) {
        if (cast<IntegerType>(opInst->getType())->getBitWidth() > 8) {
          auto op0 = dyn_cast<Instruction>(opInst->getOperand(0));
          if (op0->getOpcode() != Instruction::SExt)
            continue;

          auto op0in = dyn_cast<Instruction>(op0->getOperand(0));
          if (cast<IntegerType>(op0in->getType())->getBitWidth() > 8)
            continue;

          auto op1 = dyn_cast<Instruction>(opInst->getOperand(1));
          if (op1->getOpcode() != Instruction::SExt)
            continue;

          auto op1in = dyn_cast<Instruction>(op1->getOperand(0));
          if (cast<IntegerType>(op1in->getType())->getBitWidth() > 8)
            continue;

          candidate.inputs.push_back(op0->getOperand(0));
          candidate.inputs.push_back(op1->getOperand(0));
          m2Branch = i;
          break;
        } else {
          m2Branch = i;
          candidate.inputs.push_back(opInst->getOperand(0));
          candidate.inputs.push_back(opInst->getOperand(1));
          break;
        }
      }
    }

    if (m2Branch < 0)
      continue;

    // Search for a_0.
    auto a0 = dyn_cast<Instruction>(I.getOperand(1 - m2Branch));

    if (a0->getOpcode() == Instruction::SExt)
      a0 = dyn_cast<Instruction>(a0->getOperand(0));

    if (a0->getOpcode() != Instruction::Add)
      continue;

    if (cast<IntegerType>(a0->getType())->getBitWidth() > 58)
      continue;

    // Search for m_0 and m_1.
    bool mulsFound = true;
    for (unsigned i = 0; i < a0->getNumOperands(); ++i) {
      auto opInst = dyn_cast<Instruction>(a0->getOperand(i));

      if (opInst->getOpcode() == Instruction::SExt)
        opInst = dyn_cast<Instruction>(opInst->getOperand(0));

      if (opInst->getOpcode() != Instruction::Mul) {
        mulsFound = false;
        break;
      }

      if (cast<IntegerType>(opInst->getType())->getBitWidth() > 8) {
        auto op0 = dyn_cast<Instruction>(opInst->getOperand(0));
        if (op0->getOpcode() != Instruction::SExt) {
          mulsFound = false;
          break;
        }

        auto op0in = dyn_cast<Instruction>(op0->getOperand(0));
        if (cast<IntegerType>(op0in->getType())->getBitWidth() > 8) {
          mulsFound = false;
          break;
        }

        auto op1 = dyn_cast<Instruction>(opInst->getOperand(1));
        if (op1->getOpcode() != Instruction::SExt) {
          mulsFound = false;
          break;
        }

        auto op1in = dyn_cast<Instruction>(op1->getOperand(0));
        if (cast<IntegerType>(op1in->getType())->getBitWidth() > 8) {
          mulsFound = false;
          break;
        }

        candidate.inputs.push_back(op0->getOperand(0));
        candidate.inputs.push_back(op1->getOperand(0));
      } else {
        candidate.inputs.push_back(opInst->getOperand(0));
        candidate.inputs.push_back(opInst->getOperand(1));
      }
    }
    candidateInsts.push_back(candidate);
  }
}

void replaceCandidateWithDotProdCall(CandidateInst &candidate,
                                     Function *DotProdFunc,
                                     LLVMContext &context) {
  auto output = dyn_cast<Instruction>(candidate.outputs[0]);

  IRBuilder<> builder(output);

  Value *args[6];
  Value **argPtr = &args[0];
  for (auto &input : candidate.inputs)
    *(argPtr++) = input;

  Value *dotProd = builder.CreateCall(DotProdFunc, args);

  auto outType = dyn_cast<IntegerType>(output->getType());
  auto outSize = outType->getBitWidth();
  // Replace the add instruction with the result
  if (outSize < 58)
    dotProd = builder.CreateTrunc(dotProd, outType);

  output->replaceAllUsesWith(dotProd);
  output->eraseFromParent();
}

bool DotProdize::runOnBasicBlock(BasicBlock &BB) {
  Function *F = BB.getParent();
  if (F->getName().startswith("_ssdm_op") || F->getName() == "dotprod")
    return false;

  // Get the SIMD function
  Module *module = F->getParent();
  Function *DotProdFunc = module->getFunction("dotprod");
  assert(DotProdFunc && "dotprod function not found");

  LLVMContext &context = F->getContext();

  std::list<CandidateInst> candidates;
  getCandidates(BB, candidates);

  SmallVector<SmallVector<Instruction *, 8>, 8> mulLeafsCandidates;
  SmallVector<SmallVector<Instruction *, 8>, 8> addInternalCandidates;
  SmallVector<Instruction *, 8> addRoots;
  // Iterate in reverse order to avoid collecting subset trees.
  for (auto II = BB.end(), IB = BB.begin(); II != IB; --II) {
    Instruction *I = II;
    if (I->getOpcode() == Instruction::Add) {
      bool subset = false;
      for (auto &addInternal : addInternalCandidates) {
        for (auto &add : addInternal) {
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

      SmallVector<Instruction *, 8> mulLeafs;
      SmallVector<Instruction *, 8> addInternal;
      if (getDotProdTree(I, mulLeafs, addInternal)) {
        mulLeafsCandidates.push_back(mulLeafs);
        addInternalCandidates.push_back(addInternal);
        addRoots.push_back(I);
      }
    }
  }

  for (auto &candidate : candidates)
    replaceCandidateWithDotProdCall(candidate, DotProdFunc, context);

  return (candidates.size() > 0);
}

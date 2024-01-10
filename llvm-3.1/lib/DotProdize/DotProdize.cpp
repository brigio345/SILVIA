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
  SmallVector<Instruction *, 2> inInsts;
  SmallVector<Instruction *, 1> outInsts;
};

char DotProdize::ID = 0;
static RegisterPass<DotProdize> X("dot-prod-ize",
                                  "Dot products to DSP58 dot product mode.",
                                  false /* Only looks at CFG */,
                                  true /* Transformation Pass */);

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

    candidate.outInsts.push_back(&I);

    // Search for m_2.
    auto m2Branch = -1;

    for (unsigned i = 0; i < I.getNumOperands(); ++i) {
      auto opInst = dyn_cast<Instruction>(I.getOperand(i));

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

          candidate.inInsts.push_back(op0);
          candidate.inInsts.push_back(op1);
          m2Branch = i;
          break;
        } else {
          m2Branch = i;
          candidate.inInsts.push_back(opInst);
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

        candidate.inInsts.push_back(op0);
        candidate.inInsts.push_back(op1);
      } else {
        candidate.inInsts.push_back(opInst);
      }
    }
    candidateInsts.push_back(candidate);
  }
}

bool DotProdize::runOnBasicBlock(BasicBlock &BB) {
  Function *F = BB.getParent();
  if (F->getName().startswith("_ssdm_op") || F->getName() == "dotprod")
    return false;

  // Get the SIMD function
  Module *module = F->getParent();
  // Function *DotProdFunc = module->getFunction("dotprod");
  // assert(DotProdFunc && "SIMD function not found");

  LLVMContext &context = F->getContext();

  bool modified = false;

  std::list<CandidateInst> candidateInsts;
  getCandidates(BB, candidateInsts);

  for (auto &candidate : candidateInsts) {
    // TODO: replace each candidate with a call to dotprod
  }

  return (candidateInsts.size() > 0);
}

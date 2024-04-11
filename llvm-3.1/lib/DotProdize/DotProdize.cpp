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

char DotProdize::ID = 0;
static RegisterPass<DotProdize> X("dot-prod-ize",
                                  "Dot products to DSP58 dot product mode.",
                                  false /* Only looks at CFG */,
                                  true /* Transformation Pass */);

struct DotProdTree {
  Instruction *addRoot;
  SmallVector<Instruction *, 8> addInternal;
  SmallVector<Instruction *, 8> mulLeafs;
};

bool getDotProdTree(Instruction *addRoot, DotProdTree &tree) {
  if (addRoot->getOpcode() != Instruction::Add)
    return false;

  tree.addRoot = addRoot;
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
    // TODO: Accept multiple uses too. We need to split the chain in that point.
    if (!op->hasOneUse())
      return false;

    switch (op->getOpcode()) {
    case Instruction::Mul:
      tree.mulLeafs.push_back(op);
      break;
    case Instruction::Add:
      if (!getDotProdTree(op, tree))
        return false;
      tree.addInternal.push_back(op);
      break;
    default:
      return false;
    }
  }

  return true;
}

void replaceTreeWithDotProds(DotProdTree &tree, Function *dotProd,
                             LLVMContext &context) {
  IRBuilder<> builder(tree.addRoot);

  SmallVector<Instruction *, 3> muls;
  Instruction *rootNew = nullptr;
  for (unsigned i = 0; i < tree.mulLeafs.size();) {
    Instruction *rootCurr = nullptr;
    if ((i + 3) <= tree.mulLeafs.size()) {
      SmallVector<Value *, 6> args;
      for (unsigned j = 0; j < 3; ++j) {
        args.push_back(cast<Instruction>(
            builder.CreateTrunc(tree.mulLeafs[i + j]->getOperand(0),
                                IntegerType::get(context, 8))));
        args.push_back(cast<Instruction>(
            builder.CreateTrunc(tree.mulLeafs[i + j]->getOperand(1),
                                IntegerType::get(context, 8))));
      }
      rootCurr = cast<Instruction>(builder.CreateSExt(
          builder.CreateCall(dotProd, args), IntegerType::get(context, 58)));

      i += 3;
    } else {
      rootCurr = cast<Instruction>(
          builder.CreateSExt(tree.mulLeafs[i], IntegerType::get(context, 58)));

      i++;
    }

    rootNew = (rootNew ? cast<Instruction>(builder.CreateAdd(rootNew, rootCurr))
                       : rootCurr);
  }

  tree.addRoot->replaceAllUsesWith(
      builder.CreateTrunc(rootNew, tree.addRoot->getType()));
  tree.addRoot->eraseFromParent();
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

  SmallVector<DotProdTree, 8> candidates;
  // Iterate in reverse order to avoid collecting subset trees.
  for (auto II = BB.end(), IB = BB.begin(); II != IB; --II) {
    Instruction *I = II;
    if (I->getOpcode() == Instruction::Add) {
      bool subset = false;
      for (auto candidate : candidates) {
        for (auto &add : candidate.addInternal) {
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

      DotProdTree tree;
      if (getDotProdTree(I, tree))
        candidates.push_back(tree);
    }
  }

  for (unsigned i = 0; i < candidates.size(); ++i)
    replaceTreeWithDotProds(candidates[i], DotProdFunc, context);

  return (candidates.size() > 0);
}

#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"

using namespace llvm;

struct SIMDAdd : public FunctionPass {
	static char ID;
	SIMDAdd() : FunctionPass(ID) {}

	bool runOnFunction(Function &F) override;
};

char SIMDAdd::ID = 0;
static RegisterPass<SIMDAdd> X("simd-add", "Map add instructions to SIMD DSPs",
		false /* Only looks at CFG */,
		true /* Transformation Pass */);

bool SIMDAdd::runOnFunction(Function &F) {
	if (skipFunction(F))
		return false;

	for (auto &BB : F) {
		// collect all the add instructions
		std::vector<Instruction *> addInstrs;
		for (auto &I : BB) {
			if (auto *binOp = dyn_cast<BinaryOperator>(&I)) {
				if (binOp->getOpcode() == Instruction::Add) {
					errs() << "add instruction found\n";
					addInstrs.push_back(binOp);
				}
			}
		}
	}

	return false;
}


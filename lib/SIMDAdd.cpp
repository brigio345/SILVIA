#include "llvm/IR/Function.h"
#include "llvm/Pass.h"

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

	return false;
}


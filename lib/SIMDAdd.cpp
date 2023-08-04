#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IRBuilder.h"

#include <list>
#include <map>

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

// TODO: Maybe use DominatorTree? (It may be an overkill)
// DominatorTree DT(F);
// if (DT.dominates(inst0, inst1)) {...}
bool isBeforeInBB(Instruction *inst0, Instruction *inst1) {
	if (inst0 == inst1)
		return false;

	// There is no ordering between instructions belonging to different basic blocks
	if (inst0->getParent() != inst1->getParent())
		return false;

	for (auto &inst : *inst0->getParent()) {
		// inst0 found before finding inst1
		if (&inst == inst0)
			return true;

		// inst1 found before finding inst0
		if (&inst == inst1)
			return false;
	}

	// Execution should never reach here.
	return false;
}

Instruction *getLastOperandDef(Instruction *inst, std::map<Instruction *, int> &instMap) {
	Instruction *lastDef = nullptr;

	for (auto &op : inst->operands()) {
		if (Instruction *opInst = dyn_cast<Instruction>(op)) {
			if ((!lastDef) || (instMap[lastDef] < instMap[opInst]))
				lastDef = opInst;
		}
	}

	return lastDef;
}

Instruction *getFirstValueUse(Instruction *inst, std::map<Instruction *, int> &instMap) {
	Instruction *firstUse = nullptr;

	for (User *user : inst->users()) {
		if (Instruction *userInst = dyn_cast<Instruction>(user)) {
			if ((!firstUse) || (instMap[userInst] < instMap[firstUse]))
				firstUse = userInst;
		}
	}

	return firstUse;
}

Function *declareAddFunction(Function &F) {
	LLVMContext &context = F.getContext();

	// Create the function type for add_4simd: i48 (i48, i48)
	std::vector<Type*> paramTypes = {IntegerType::get(context, 48),
		IntegerType::get(context, 48)};
	FunctionType *myAddType = FunctionType::get(IntegerType::get(context, 48),
			paramTypes, false);

	// Create the function declaration for myAdd
	Function *myAddFunc = Function::Create(myAddType,
			Function::ExternalLinkage, "dsp_add_4simd_pipe_l0",
			F.getParent());

	// The OptimizeNone attribute makes skipFunction(*myAddFunc)
	// return true. Therefore, it prevents from recursively replace
	// the add instruction within myAddFunc.
	// OptimizeNone requires the NoInline attribute.
	myAddFunc->addFnAttr(Attribute::NoInline);
	myAddFunc->addFnAttr(Attribute::OptimizeNone);

	// Set names for the function parameters
	(myAddFunc->arg_begin() + 0)->setName("a");
	(myAddFunc->arg_begin() + 1)->setName("b");

	// Create a new basic block to hold the function body
	BasicBlock *entryBB = BasicBlock::Create(context, "entry", myAddFunc);

	// Set the insertion point to the new basic block
	IRBuilder<> builder(entryBB);

	// Get the function arguments
	Value *a = myAddFunc->arg_begin() + 0;
	Value *b = myAddFunc->arg_begin() + 1;

	Value *a0 = builder.CreateTrunc(builder.CreateLShr(a, 36), IntegerType::get(context, 12));
	Value *a1 = builder.CreateTrunc(builder.CreateLShr(a, 24), IntegerType::get(context, 12));
	Value *a2 = builder.CreateTrunc(builder.CreateLShr(a, 12), IntegerType::get(context, 12));
	Value *a3 = builder.CreateTrunc(a, IntegerType::get(context, 12));
	Value *b0 = builder.CreateTrunc(builder.CreateLShr(b, 36), IntegerType::get(context, 12));
	Value *b1 = builder.CreateTrunc(builder.CreateLShr(b, 24), IntegerType::get(context, 12));
	Value *b2 = builder.CreateTrunc(builder.CreateLShr(b, 12), IntegerType::get(context, 12));
	Value *b3 = builder.CreateTrunc(b, IntegerType::get(context, 12));

	// Perform the addition
	Value *sum0 = builder.CreateAdd(a0, b0);
	Value *sum1 = builder.CreateAdd(a1, b1);
	Value *sum2 = builder.CreateAdd(a2, b2);
	Value *sum3 = builder.CreateAdd(a3, b3);

	Value *sum0_48 = builder.CreateSExt(sum0, IntegerType::get(context, 48));
	Value *sum1_48 = builder.CreateSExt(sum1, IntegerType::get(context, 48));
	Value *sum2_48 = builder.CreateSExt(sum2, IntegerType::get(context, 48));
	Value *sum3_48 = builder.CreateSExt(sum3, IntegerType::get(context, 48));

	Value *sum_concat = builder.CreateShl(sum0_48, 36);
	sum_concat = builder.CreateOr(sum_concat, builder.CreateShl(sum1_48, 24));
	sum_concat = builder.CreateOr(sum_concat, builder.CreateShl(sum2_48, 12));
	sum_concat = builder.CreateOr(sum_concat, sum3_48);

	// Create the return instruction
	builder.CreateRet(sum_concat);

	return myAddFunc;
}

bool SIMDAdd::runOnFunction(Function &F) {
	if (skipFunction(F))
		return false;

	// Get the LLVM context
	LLVMContext &context = F.getContext();

	bool modified = false;

	for (auto &BB : F) {
		// collect all the add instructions
		std::list<Instruction *> simd4Candidates;
		for (auto &I : BB) {
			if (auto *binOp = dyn_cast<BinaryOperator>(&I)) {
				if (binOp->getOpcode() == Instruction::Add) {
					if (cast<IntegerType>(binOp->getType())->getBitWidth() <= 12)
						simd4Candidates.push_back(binOp);
					// TODO: collect candidates for simd2
					// else if (cast<IntegerType>(binOp->getType())->getBitWidth() <= 24)
						// simd2Candidates.push_back(binOp);
				}
			}
		}

		std::map<Instruction *, int> instMap;
		for (auto &inst : BB.getInstList())
			instMap[&inst] = (instMap.size() + 1);

		// Build tuples of 4 instructions that can be mapped to the
		// same SIMD DSP.
		std::vector<std::vector<Instruction *>> instTuples;
		while (!simd4Candidates.empty()) {
			auto *addInst = simd4Candidates.front();
			simd4Candidates.pop_front();

			std::vector<Instruction *> instTuple;

			instTuple.push_back(cast<Instruction>(addInst));

			auto *lastDef = getLastOperandDef(addInst, instMap);
			auto *firstUse = getFirstValueUse(addInst, instMap);

			for (auto *addInstCurr : simd4Candidates) {
				auto *lastDefCurr = getLastOperandDef(addInstCurr, instMap);
				auto *firstUseCurr = getFirstValueUse(addInstCurr, instMap);

				// Update with tuple worst case
				if ((!lastDef) || (lastDefCurr && (instMap[lastDef] < instMap[lastDefCurr])))
					lastDef = lastDefCurr;

				if ((!firstUse) || (firstUseCurr && (instMap[firstUseCurr] < instMap[firstUse])))
					firstUse = firstUseCurr;

				// If firstUse is before lastDef this pair of
				// instructions is not compatible for optimization
				if (firstUse && (instMap[firstUse] < instMap[lastDef]))
					continue;

				instTuple.push_back(addInstCurr);

				if (instTuple.size() == 4)
					break;
			}

			for (auto inst : instTuple) {
				simd4Candidates.erase(std::remove(simd4Candidates.begin(),
						simd4Candidates.end(), inst),
						simd4Candidates.end());
			}
			instTuples.push_back(instTuple);
		}

		Function *myAddFunc = nullptr;
		if (!instTuples.empty()) {
			// Check if the add_4simd function already exists in the current module
			myAddFunc = F.getParent()->getFunction("dsp_add_4simd_pipe_l0");
			if (!myAddFunc)
				myAddFunc = declareAddFunction(F);
		}

		for (auto instTuple : instTuples) {
			// TODO: maybe also skip tuples of size 2?
			if (instTuple.size() < 2)
				continue;

			IRBuilder<> builder(instTuple[0]);

			Value *args[2];
			for (int j = 0; j < 2; j++) {
				for (int i = 0; i < instTuple.size(); i++) {
					Value *arg = builder.CreateSExt(instTuple[i]->getOperand(j), IntegerType::get(context, 48));
					int shift_amount = (12 * (3 - i));
					if (shift_amount > 0) {
						arg = builder.CreateShl(builder.CreateSExt(arg,
									IntegerType::get(context, 48)), shift_amount);
					}
					args[j] = (i > 0) ? builder.CreateOr(args[j], arg) : arg;
				}
			}

			Value *sum_concat = builder.CreateCall(myAddFunc, args);

			Value *result[4];
			for (int i = 0; i < instTuple.size(); i++) {
				int shift_amount = (12 * (3 - i));
				Value *result_shifted = (shift_amount > 0) ?
					builder.CreateLShr(sum_concat, shift_amount) : sum_concat;

				result[i] = builder.CreateTrunc(result_shifted,
						instTuple[i]->getType());
			}

			// Replace the add instruction with the result
			for (int i = 0; i < instTuple.size(); i++) {
				instTuple[i]->replaceAllUsesWith(result[i]);
				instTuple[i]->eraseFromParent();
			}
		}

		modified |= !(instTuples.empty());
	}

	return modified;
}


#include "llvm/Module.h"
#include "llvm/Pass.h"
#include "llvm/Function.h"
#include "llvm/Instructions.h"
#include "llvm/Support/IRBuilder.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/DenseMap.h"

#include <queue>

using namespace llvm;

struct SIMDAdd : public FunctionPass {
	static char ID;
	SIMDAdd() : FunctionPass(ID) {}

	bool runOnFunction(Function &F);
};

char SIMDAdd::ID = 0;
static RegisterPass<SIMDAdd> X("simd-add", "Map add instructions to SIMD DSPs",
		false /* Only looks at CFG */,
		true /* Transformation Pass */);

Instruction *getLastOperandDef(Instruction *inst, DenseMap<Instruction *, int> &instMap) {
	Instruction *lastDef = NULL;

	for (unsigned int i  = 0; i < inst->getNumOperands(); i++) {
		Value *op = inst->getOperand(i);
		if (Instruction *opInst = dyn_cast<Instruction>(op)) {
			if ((!lastDef) || (instMap[lastDef] < instMap[opInst]))
				lastDef = opInst;
		}
	}

	return lastDef;
}

Instruction *getFirstValueUse(Instruction *inst, DenseMap<Instruction *, int> &instMap) {
	Instruction *firstUse = NULL;

	for (Value::use_iterator UI = inst->use_begin(), UE = inst->use_end(); UI != UE; ++UI) {
		Value *user = *UI;
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
	SmallVector<Type *, 2> argsIn;
	argsIn.push_back(IntegerType::getIntNTy(context, 48));
	argsIn.push_back(IntegerType::getIntNTy(context, 48));
	FunctionType *myAddType = FunctionType::get(
			IntegerType::getIntNTy(context, 48),
			argsIn, false);

	// Create the function declaration for myAdd
	Function *myAddFunc = Function::Create(myAddType,
			Function::ExternalLinkage, "dsp_add_4simd_pipe_l0",
			F.getParent());

	// The OptimizeNone attribute makes skipFunction(*myAddFunc)
	// return true. Therefore, it prevents from recursively replace
	// the add instruction within myAddFunc.
	// OptimizeNone requires the NoInline attribute.
	//FIXME: check if LLVM 3.1 provides alternatives to skipFunction
	myAddFunc->addFnAttr(Attribute::NoInline);
	//myAddFunc->addFnAttr(Attribute::OptimizeNone);

	// Set names for the function parameters
	Function::arg_iterator argIt = myAddFunc->arg_begin();
	Value *tmp = argIt;
	tmp->setName("a");
	argIt++;
	tmp = argIt;
	tmp->setName("b");

	// Create a new basic block to hold the function body
	BasicBlock *entryBB = BasicBlock::Create(context, "entry", myAddFunc);

	// Set the insertion point to the new basic block
	IRBuilder<> builder(entryBB);

	// Get the function arguments
	argIt = myAddFunc->arg_begin();
	Value *a = argIt;
	argIt++;
	Value *b = argIt;

	Value *sum_concat;
	for (int i = 0; i < 4; i++) {
		int shift_amount = (12 * (3 - i));
		Value *a_shifted = (shift_amount > 0) ?
			builder.CreateLShr(a, shift_amount) : a;
		Value *a_i = builder.CreateTrunc(a_shifted,
				IntegerType::get(context, 12));

		Value *b_shifted = (shift_amount > 0) ?
			builder.CreateLShr(b, shift_amount) : b;
		Value *b_i = builder.CreateTrunc(b_shifted,
				IntegerType::get(context, 12));

		Value *sum = builder.CreateAdd(a_i, b_i);
		Value *sum_48 = builder.CreateSExt(sum, IntegerType::get(context, 48));

		sum_concat = (shift_amount > 0) ?
			builder.CreateShl(sum_48, shift_amount) :
			sum_48;
		sum_concat = builder.CreateOr(sum_concat, sum_48);
	}

	// Create the return instruction
	builder.CreateRet(sum_concat);

	return myAddFunc;
}

bool SIMDAdd::runOnFunction(Function &F) {
	//FIXME: check if LLVM 3.1 provides alternatives to skipFunction
	//if (skipFunction(F))
	if (F.getName().startswith("_ssdm_op") || F.getName() == "dsp_add_4simd_pipe_l0")
		return false;

	// Get the LLVM context
	LLVMContext &context = F.getContext();

	bool modified = false;

	for (Function::iterator FI = F.begin(), FE = F.end(); FI != FE; ++FI) {
		BasicBlock &BB = *FI;
		// collect all the add instructions
		std::queue<Instruction *> simd4Candidates;
		for (BasicBlock::iterator BI = BB.begin(), BE = BB.end(); BI != BE; ++BI) {
			Instruction *I = BI;
			if (I->getOpcode() == Instruction::Add) {
				if (cast<IntegerType>(I->getType())->getBitWidth() <= 12)
					simd4Candidates.push(I);
				// TODO: collect candidates for simd2
				// else if (cast<IntegerType>(binOp->getType())->getBitWidth() <= 24)
					// simd2Candidates.push_back(binOp);
			}
		}

		// TODO: Maybe use DominatorTree? (It may be an overkill)
		// DominatorTree DT(F);
		// if (DT.dominates(inst0, inst1)) {...}
		DenseMap<Instruction *, int> instMap;
		for (BasicBlock::iterator BI = BB.begin(), BE = BB.end(); BI != BE; ++BI) {
			Instruction *inst = BI;
			instMap[inst] = (instMap.size() + 1);
		}

		// Build tuples of 4 instructions that can be mapped to the
		// same SIMD DSP.
		// TODO: check if a size of 8 is a good choice
		SmallVector<SmallVector<Instruction *, 4>, 8> instTuples;
		while (!simd4Candidates.empty()) {
			SmallVector<Instruction *, 4> instTuple;
			Instruction *lastDef = NULL;
			Instruction *firstUse = NULL;

			for (unsigned i = 0; i < simd4Candidates.size(); i++) {
				Instruction *addInstCurr = simd4Candidates.front();
				simd4Candidates.pop();

				Instruction *lastDefCurr = getLastOperandDef(addInstCurr, instMap);
				Instruction *firstUseCurr = getFirstValueUse(addInstCurr, instMap);

				// Update with tuple worst case
				if ((!lastDef) || (lastDefCurr && (instMap[lastDef] < instMap[lastDefCurr])))
					lastDef = lastDefCurr;

				if ((!firstUse) || (firstUseCurr && (instMap[firstUseCurr] < instMap[firstUse])))
					firstUse = firstUseCurr;

				// If firstUse is before lastDef this pair of
				// instructions is not compatible with current
				// tuple.
				if (firstUse && (instMap[firstUse] < instMap[lastDef])) {
					simd4Candidates.push(addInstCurr);
					continue;
				}

				instTuple.push_back(addInstCurr);

				if (instTuple.size() == 4)
					break;
			}

			// TODO: maybe also skip tuples of size 2?
			if (instTuple.size() > 1)
				instTuples.push_back(instTuple);
		}

		Function *myAddFunc = NULL;
		if (!instTuples.empty()) {
			// Check if the add_4simd function already exists in the current module
			Module *module = F.getParent();
			myAddFunc = module->getFunction("dsp_add_4simd_pipe_l0");
			if (!myAddFunc)
				myAddFunc = declareAddFunction(F);
		}

		for (unsigned i = 0; i < instTuples.size(); i++) {
			SmallVector<Instruction *, 4> instTuple = instTuples[i];
			IRBuilder<> builder(instTuple[0]);

			Value *args[2];
			for (unsigned j = 0; j < 2; j++) {
				for (unsigned i = 0; i < instTuple.size(); i++) {
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
			for (unsigned i = 0; i < instTuple.size(); i++) {
				int shift_amount = (12 * (3 - i));
				Value *result_shifted = (shift_amount > 0) ?
					builder.CreateLShr(sum_concat, shift_amount) : sum_concat;

				result[i] = builder.CreateTrunc(result_shifted,
						instTuple[i]->getType());
			}

			// Replace the add instruction with the result
			for (unsigned i = 0; i < instTuple.size(); i++) {
				instTuple[i]->replaceAllUsesWith(result[i]);
				instTuple[i]->eraseFromParent();
			}
		}

		modified |= !(instTuples.empty());
	}

	return modified;
}


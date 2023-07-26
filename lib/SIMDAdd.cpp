#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IRBuilder.h"

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

	// Get the LLVM context
	LLVMContext &context = F.getContext();

	// Check if the myAdd function already exists in the current module
	Function *myAddFunc = F.getParent()->getFunction("myAdd");
	if (!myAddFunc) {
		// Create the function type for myAdd: void (i32, i32, i32*)
		std::vector<Type*> paramTypes = {Type::getInt32Ty(context),
			Type::getInt32Ty(context), Type::getInt32PtrTy(context)};
		FunctionType *myAddType = FunctionType::get(Type::getVoidTy(context),
				paramTypes, false);

		// Create the function declaration for myAdd
		myAddFunc = Function::Create(myAddType, Function::ExternalLinkage,
				"myAdd", F.getParent());

		// The OptimizeNone attribute makes skipFunction(*myAddFunc)
		// return true. Therefore, it prevents from recursively replace
		// the add instruction within myAddFunc.
		// OptimizeNone requires the NoInline attribute.
		myAddFunc->addFnAttr(Attribute::NoInline);
		myAddFunc->addFnAttr(Attribute::OptimizeNone);

		// Set names for the function parameters
		myAddFunc->arg_begin()->setName("a");
		(myAddFunc->arg_begin() + 1)->setName("b");
		(myAddFunc->arg_begin() + 2)->setName("c");

		// Create a new basic block to hold the function body
		BasicBlock *entryBB = BasicBlock::Create(context, "entry", myAddFunc);

		// Set the insertion point to the new basic block
		IRBuilder<> builder(entryBB);

		// Get the function arguments
		Value *a = myAddFunc->arg_begin();
		Value *b = myAddFunc->arg_begin() + 1;
		Value *c = myAddFunc->arg_begin() + 2;

		// Perform the addition
		Value *sum = builder.CreateAdd(a, b);

		// Store the sum in the reference parameter c
		builder.CreateStore(sum, c);

		// Create the return instruction
		builder.CreateRetVoid();
	}

	bool modified = false;

	for (auto &BB : F) {
		// collect all the add instructions
		std::vector<Instruction *> addInsts;
		for (auto &I : BB) {
			if (auto *binOp = dyn_cast<BinaryOperator>(&I)) {
				if (binOp->getOpcode() == Instruction::Add) {
					errs() << "add instruction found\n";
					if (cast<IntegerType>(binOp->getType())->getBitWidth() == 32)
						addInsts.push_back(binOp);
				}
			}
		}

		for (auto *addInst : addInsts) {
			// Get the operands of the add instruction
			Value *op1 = addInst->getOperand(0);
			Value *op2 = addInst->getOperand(1);

			// Create the call to myAdd function
			IRBuilder<> builder(addInst);
			Value *c = builder.CreateAlloca(Type::getInt32Ty(context));
			Value *args[] = {op1, op2, c};
			builder.CreateCall(myAddFunc, args);

			// Assign the return value passed by reference to a variable
			Value *result = builder.CreateLoad(Type::getInt32Ty(context), c);

			// Replace the add instruction with the result
			addInst->replaceAllUsesWith(result);
			addInst->eraseFromParent();

			errs() << "add i32 instruction replaced\n";
		}

		modified |= !(addInsts.empty());
	}

	return modified;
}


#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/BasicBlock.h"
#include "llvm/Function.h"
#include "llvm/Instructions.h"
#include "llvm/Module.h"
#include "llvm/Pass.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/IRBuilder.h"

#include <cassert>
#include <list>

using namespace llvm;

struct SIMDAdd : public BasicBlockPass {
  static char ID;
  SIMDAdd() : BasicBlockPass(ID) {}

  bool runOnBasicBlock(BasicBlock &BB) override;

  virtual void getAnalysisUsage(AnalysisUsage &AU) const {
    AU.addRequired<AliasAnalysis>();
    AU.addRequired<LoopInfo>();
  }

  bool anticipateDefs(Instruction *inst, bool anticipateInst);
  bool posticipateUses(Instruction *inst, bool posticipateInst);
  bool isMoveMemSafe(Instruction *instToMove, Instruction *firstInst,
                     Instruction *lastInst);

  AliasAnalysis *AA;
  LoopInfo *LI;
};

struct CandidateInst {
  SmallVector<Instruction *, 2> inInsts;
  SmallVector<Instruction *, 1> outInsts;
};

char SIMDAdd::ID = 0;
static RegisterPass<SIMDAdd> X("simd-add", "Map add instructions to SIMD DSPs",
                               false /* Only looks at CFG */,
                               true /* Transformation Pass */);

static cl::opt<std::string>
    SIMDOp("simd-add-op", cl::init("add4simd"), cl::Hidden,
           cl::desc("The operation to map to SIMD DSPs. "
                    "Possible values are: add4simd."));

bool dependsOn(Instruction *inst0, Instruction *inst1) {
  if (inst0->getParent() != inst1->getParent())
    return false;

  if (inst0 == inst1)
    return true;

  for (int i = 0; i < inst0->getNumOperands(); ++i) {
    if (auto operandInst = dyn_cast<Instruction>(inst0->getOperand(i))) {
      if (dependsOn(operandInst, inst1))
        return true;
    }
  }

  return false;
}

void getInstMap(BasicBlock *BB, DenseMap<Instruction *, int> &instMap) {
  for (auto &inst : *BB)
    instMap[&inst] = instMap.size();
}

Instruction *getLastOperandDef(Instruction *inst) {
  BasicBlock *instBB = inst->getParent();

  DenseMap<Instruction *, int> instMap;
  getInstMap(instBB, instMap);

  Instruction *lastDef = nullptr;
  for (unsigned i = 0; i < inst->getNumOperands(); ++i) {
    Value *op = inst->getOperand(i);
    auto opInst = dyn_cast<Instruction>(op);
    if ((!opInst) || (opInst->getParent() != instBB))
      continue;
    if ((!lastDef) || (instMap[lastDef] < instMap[opInst]))
      lastDef = opInst;
  }

  return lastDef;
}

Instruction *getFirstValueUse(Instruction *inst) {
  BasicBlock *instBB = inst->getParent();

  DenseMap<Instruction *, int> instMap;
  getInstMap(instBB, instMap);

  Instruction *firstUse = nullptr;
  for (auto UI = inst->use_begin(), UE = inst->use_end(); UI != UE; ++UI) {
    Value *user = *UI;
    auto userInst = dyn_cast<Instruction>(user);
    if ((!userInst) || (userInst->getParent() != instBB))
      continue;
    if ((!firstUse) || (instMap[userInst] < instMap[firstUse]))
      firstUse = userInst;
  }

  return firstUse;
}

bool SIMDAdd::isMoveMemSafe(Instruction *instToMove, Instruction *firstInst,
                            Instruction *lastInst) {
  auto loadToMove = dyn_cast<LoadInst>(instToMove);
  auto storeToMove = dyn_cast<StoreInst>(instToMove);

  if ((!loadToMove) && (!storeToMove))
    return true;

  AliasAnalysis::Location locToMove =
      (storeToMove ? AA->getLocation(storeToMove)
                   : AA->getLocation(loadToMove));

  bool toCheck = false;
  for (auto &I : *(instToMove->getParent())) {
    // Skip the instructions before the interval involved in the movement.
    if ((!toCheck) && (&I != firstInst))
      continue;
    toCheck = true;

    // Skip the instructions after the interval involved in the movement.
    if (&I == lastInst)
      break;

    if (dyn_cast<CallInst>(&I))
      return false;

    if (auto store = dyn_cast<StoreInst>(&I)) {
      auto loc = AA->getLocation(store);

      if (AA->alias(locToMove, loc) != AliasAnalysis::AliasResult::NoAlias)
        return false;
    }

    // If a load aliases with another load is not an issue. There is no need to
    // check.
    if (loadToMove)
      continue;

    if (auto load = dyn_cast<LoadInst>(&I)) {
      auto loc = AA->getLocation(load);

      if (AA->alias(locToMove, loc) != AliasAnalysis::AliasResult::NoAlias)
        return false;
    }
  }

  return true;
}

bool SIMDAdd::anticipateDefs(Instruction *inst, bool anticipateInst = false) {
  // TODO: Anticipate calls if not crossing other calls or loads/stores.
  auto opcode = inst->getOpcode();
  if ((opcode == Instruction::PHI) || (opcode == Instruction::Call))
    return false;

  auto modified = false;
  BasicBlock *instBB = inst->getParent();
  for (unsigned i = 0; i < inst->getNumOperands(); ++i) {
    Value *op = inst->getOperand(i);
    auto opInst = dyn_cast<Instruction>(op);
    if (!opInst)
      continue;
    if (opInst->getParent() == instBB)
      modified = anticipateDefs(opInst, true);
  }

  if (!anticipateInst)
    return modified;

  Instruction *insertionPoint = instBB->getFirstNonPHI();
  auto lastDef = getLastOperandDef(inst);
  if (lastDef && (lastDef->getOpcode() != Instruction::PHI))
    insertionPoint = lastDef->getNextNode();

  if (!isMoveMemSafe(inst, insertionPoint, inst))
    return modified;

  inst->moveBefore(insertionPoint);

  return true;
}

bool SIMDAdd::posticipateUses(Instruction *inst, bool posticipateInst = false) {
  // TODO: Posticipate calls if not crossing other calls or loads/stores.
  auto opcode = inst->getOpcode();
  if ((opcode == Instruction::PHI) || (opcode == Instruction::Call))
    return false;

  auto modified = false;
  BasicBlock *instBB = inst->getParent();
  for (auto UI = inst->use_begin(), UE = inst->use_end(); UI != UE; ++UI) {
    Value *user = *UI;
    auto userInst = dyn_cast<Instruction>(user);
    if (!userInst)
      continue;
    if (userInst->getParent() == instBB)
      modified = posticipateUses(userInst, true);
  }

  if (!posticipateInst)
    return modified;

  Instruction *insertionPoint = getFirstValueUse(inst);
  if (!insertionPoint)
    insertionPoint = instBB->getTerminator();

  if (!isMoveMemSafe(inst, inst, insertionPoint))
    return modified;

  inst->moveBefore(insertionPoint);

  return true;
}

// Collect all the add instructions.
void getSIMDableInstructions(BasicBlock &BB,
                             std::list<CandidateInst> &candidateInsts) {
  for (auto &I : BB) {
    if (I.getOpcode() != Instruction::Add)
      continue;
    if (isa<Constant>(I.getOperand(0)) || isa<Constant>(I.getOperand(1)))
      continue;
    if (cast<IntegerType>(I.getType())->getBitWidth() <= 12) {
      CandidateInst candidate;
      candidate.inInsts.push_back(&I);
      candidate.outInsts.push_back(&I);
      candidateInsts.push_back(candidate);
    }
    // TODO: collect candidates for simd2
    // else if (cast<IntegerType>(binOp->getType())->getBitWidth() <= 24)
    // simd2Candidates.push_back(binOp);
  }
}

void replaceInstsWithSIMDCall(SmallVector<CandidateInst, 4> instTuple,
                              Instruction *insertBefore, Function *SIMDFunc,
                              LLVMContext &context) {
  IRBuilder<> builder(insertBefore);

  SmallVector<Value *, 8> args(
      8, ConstantInt::get(IntegerType::get(context, 12), 0));
  std::string retName = "";
  for (unsigned i = 0; i < instTuple.size(); ++i) {
    retName = retName + ((retName == "") ? "" : "_") +
              instTuple[i].outInsts[0]->getName().str();
    for (unsigned j = 0; j < instTuple[i].inInsts[0]->getNumOperands(); ++j) {
      auto operand = instTuple[i].inInsts[0]->getOperand(j);

      auto arg =
          (cast<IntegerType>(operand->getType())->getBitWidth() < 12)
              ? builder.CreateZExt(operand, IntegerType::get(context, 12),
                                   operand->getName() + "_zext")
              : operand;
      args[i * instTuple[i].inInsts[0]->getNumOperands() + j] = arg;
    }
  }

  Value *sumAggr = builder.CreateCall(SIMDFunc, args, retName);

  Value *result[4];
  for (int i = 0; i < instTuple.size(); ++i) {
    result[i] = builder.CreateExtractValue(
        sumAggr, i, instTuple[i].outInsts[0]->getName() + "_zext");
    if (cast<IntegerType>(instTuple[i].outInsts[0]->getType())->getBitWidth() <
        12)
      result[i] =
          builder.CreateTrunc(result[i], instTuple[i].outInsts[0]->getType());
  }

  // Replace the add instruction with the result
  for (unsigned i = 0; i < instTuple.size(); ++i) {
    auto resName = instTuple[i].outInsts[0]->getName();

    instTuple[i].outInsts[0]->replaceAllUsesWith(result[i]);
    instTuple[i].outInsts[0]->eraseFromParent();

    result[i]->setName(resName);
  }
}

bool isLoopPipeline(Loop *L) {
  auto terminator = L->getLoopLatch()->getTerminator();
  if (auto loopMD = terminator->getMetadata("llvm.loop")) {
    for (int i = 0; i < loopMD->getNumOperands(); ++i) {
      auto pipelineEnableMD = dyn_cast<MDNode>(loopMD->getOperand(i));
      if (!pipelineEnableMD)
        continue;
      auto pipelineEnableName =
          dyn_cast<MDString>(pipelineEnableMD->getOperand(0));
      if (!pipelineEnableName)
        continue;
      if (pipelineEnableName->getString() != "llvm.loop.pipeline.enable")
        continue;

      assert(((pipelineEnableMD->getNumOperands() == 6) ||
              (pipelineEnableMD->getNumOperands() == 5)) &&
             "Unexpected number of operands for llvm.loop.pipeline.enable");
      return (pipelineEnableMD->getNumOperands() == 6);
    }
  }
  return false;
}

bool SIMDAdd::runOnBasicBlock(BasicBlock &BB) {
  LI = &getAnalysis<LoopInfo>();

  // TODO: Vitis HLS fails inserting ap_ctrl_none blackboxes in non-pipelined
  // regions.
  // To properly fix this issue we should define an ap_ctrl_hs blacbox and
  // select it when BB belongs to a non-pipelined region.
  // Moreover, we should also implement the check for pipelined functions.
  if (auto L = LI->getLoopFor(&BB)) {
    if (!isLoopPipeline(L))
      return false;
  }

  // FIXME: check if LLVM 3.1 provides alternatives to skipFunction
  // if (skipFunction(F))
  Function *F = BB.getParent();
  if (F->getName().startswith("_ssdm_op") || F->getName() == SIMDOp)
    return false;

  // Get the SIMD function
  Module *module = F->getParent();
  Function *SIMDFunc = module->getFunction(SIMDOp);
  assert(SIMDFunc && "SIMD function not found");

  LLVMContext &context = F->getContext();

  AA = &getAnalysis<AliasAnalysis>();

  bool modified = false;

  std::list<CandidateInst> candidateInsts;
  getSIMDableInstructions(BB, candidateInsts);

  candidateInsts.reverse();
  for (auto &candidateInstCurr : candidateInsts)
    modified |= anticipateDefs(candidateInstCurr.inInsts[0]);
  candidateInsts.reverse();
  for (auto &candidateInstCurr : candidateInsts)
    modified |= posticipateUses(candidateInstCurr.outInsts[0]);

  // Build tuples of 4 instructions that can be mapped to the
  // same SIMD DSP.
  // TODO: check if a size of 8 is a good choice
  while (!candidateInsts.empty()) {
    SmallVector<CandidateInst, 4> instTuple;
    Instruction *lastDef = nullptr;
    Instruction *firstUse = nullptr;

    DenseMap<Instruction *, int> instMap;
    getInstMap(&BB, instMap);
    for (auto CI = candidateInsts.begin(), CE = candidateInsts.end();
         CI != CE;) {
      CandidateInst candidateInstCurr = *CI;
      Instruction *lastDefCurr =
          getLastOperandDef(candidateInstCurr.inInsts[0]);
      Instruction *firstUseCurr =
          getFirstValueUse(candidateInstCurr.outInsts[0]);

      if ((!lastDefCurr) ||
          (lastDef && (instMap[lastDefCurr] < instMap[lastDef])))
        lastDefCurr = lastDef;

      if ((!firstUseCurr) ||
          (firstUse && (instMap[firstUse] < instMap[firstUseCurr])))
        firstUseCurr = firstUse;

      // If firstUseCurr is before lastDefCurr this pair of instructions is not
      // compatible with current tuple.
      if (firstUseCurr && lastDefCurr &&
          (instMap[firstUseCurr] < instMap[lastDefCurr])) {
        CI++;
        continue;
      }

      auto compatible = true;
      auto opInst = dyn_cast<Instruction>(candidateInstCurr.inInsts[0]);
      for (auto selected : instTuple) {
        if (dependsOn(opInst, selected.outInsts[0])) {
          compatible = false;
          break;
        }
      }
      if (!compatible) {
        CI++;
        continue;
      }

      instTuple.push_back(candidateInstCurr);

      // Update with tuple worst case
      firstUse = firstUseCurr;
      lastDef = lastDefCurr;

      // The current candidate was selected: it is not a candidate anymore.
      candidateInsts.erase(CI++);

      if (instTuple.size() == 4)
        break;
    }

    // TODO: maybe also skip tuples of size 2?
    if (instTuple.size() < 2)
      continue;

    replaceInstsWithSIMDCall(instTuple, firstUse, SIMDFunc, context);
    modified = true;
  }

  return modified;
}

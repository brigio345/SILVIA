#include "llvm/Function.h"
#include "llvm/Instructions.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"

#include <string>

using namespace llvm;

struct BBToDot : public FunctionPass {
  static char ID;
  BBToDot() : FunctionPass(ID) {}

  bool runOnFunction(Function &F) override;
};

char BBToDot::ID = 0;
static RegisterPass<BBToDot> X("bb-to-dot", "Dump BBs to DOT graphs.", false,
                               false);

bool BBToDot::runOnFunction(Function &F) {
  std::string digraphName = F.getName();
  std::replace(digraphName.begin(), digraphName.end(), '.', '_');

  std::string fileName = digraphName;
  fileName.append(".dot");
  std::string errorMsg;
  raw_fd_ostream File(fileName.c_str(), errorMsg);

  File << "digraph " << digraphName << " {\n";
  for (auto &BB : F) {
    std::string subgraphName = BB.getName();
    std::replace(subgraphName.begin(), subgraphName.end(), '.', '_');
    File << "subgraph cluster_" << subgraphName << " {\n";
    File << "label = \"" << BB.getName() << "\";\n";
    File << "style = rounded;\n";
    for (auto &I : BB) {
      auto i = reinterpret_cast<std::uintptr_t>(&I);
      std::string nodeName = "node" + std::to_string(i);
      File << nodeName << "[label=\"";
      if (I.getName() != "")
        File << I.getName() << " = ";
      File << I.getOpcodeName() << " ";
      I.getType()->print(File);
      for (auto i = 0; i < I.getNumOperands(); ++i)
        File << " " << I.getOperand(i)->getName();
      File << "\"";
      switch (I.getOpcode()) {
      case Instruction::Add:
        File << ", style=filled, color=darkolivegreen1";
        break;
      case Instruction::Mul:
        File << ", style=filled, color=coral1";
        break;
      case Instruction::SExt:
        File << ", style=filled, color=azure2";
        break;
      }
      File << "];\n";
    }
    File << "};\n";
    for (auto &I : BB) {
      auto i = reinterpret_cast<std::uintptr_t>(&I);
      std::string nodeName = "node" + std::to_string(i);
      for (auto i = 0; i < I.getNumOperands(); ++i) {
        auto op = dyn_cast<Instruction>(I.getOperand(i));
        if (!op)
          continue;

        auto id = reinterpret_cast<std::uintptr_t>(op);
        std::string opNodeName = "node" + std::to_string(id);
        File << opNodeName << " -> " << nodeName << ";\n";
      }
    }
  }
  File << "}\n";

  File.close();

  return false;
}

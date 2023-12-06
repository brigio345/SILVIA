# /bin/bash 

LLVM_NM=$1
LLVM_LINK=$2
LLVM_OPT=$3
LLVM_INPUT=$4
LLVM_OUTPUT=$5
LLVM_PASS_DIR=$6
BB_BC=$7

$LLVM_NM $LLVM_INPUT | grep giovanni > /dev/null
if [ ! $? -eq 0 ]; then
  $LLVM_LINK $LLVM_INPUT $BB_BC -o - | \
    $LLVM_OPT -mem2reg -load \
    $LLVM_PASS_DIR/LLVMCustomPasses.so -insert_dummy_bb \
    - -o $LLVM_OUTPUT
else 
  $LLVM_OPT -mem2reg -load \
  $LLVM_PASS_DIR/LLVMCustomPasses.so -insert_dummy_bb \
  $LLVM_INPUT -o $LLVM_OUTPUT
fi

# /bin/bash 

LLVM_NM=$1
LLVM_LINK=$2
LLVM_OPT=$3
LLVM_INPUT=$4
LLVM_OUTPUT=$5
LLVM_PASS_DIR=$6
SOLUTION_DIR=$7
BB_NAME=$8

BB_BC=$SOLUTION_DIR/.autopilot/db/$BB_NAME.g.bc

$LLVM_NM $LLVM_INPUT | grep $BB_NAME > /dev/null
if [ ! $? -eq 0 ]; then
  $LLVM_LINK $LLVM_INPUT $BB_BC -o - | \
    $LLVM_OPT -mem2reg -load \
    $LLVM_PASS_DIR/LLVMCustomPasses.so -insert_dummy_bb \
    -insert-dummy-bb-fn $BB_NAME - -o $LLVM_OUTPUT
else 
  $LLVM_OPT -mem2reg -load \
  $LLVM_PASS_DIR/LLVMCustomPasses.so -insert_dummy_bb \
  -insert-dummy-bb-fn $BB_NAME $LLVM_INPUT -o $LLVM_OUTPUT
fi

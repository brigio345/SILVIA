# /bin/bash 

LLVM_NM=$1
LLVM_LINK=$2
LLVM_OPT=$3
LLVM_INPUT=$4
LLVM_OUTPUT=$5
LLVM_PASS_DIR=$6
SOLUTION_DIR=$7
BB_NAME=$8
TOP_NAME=$9

BB_BC=$SOLUTION_DIR/.autopilot/db/$BB_NAME.g.bc

$LLVM_NM $LLVM_INPUT | grep giovanni > /dev/null
if [ ! $? -eq 0 ]; then
  $LLVM_LINK $LLVM_INPUT $BB_BC -o - | \
    $LLVM_OPT -mem2reg -load \
    $LLVM_PASS_DIR/LLVMCustomPasses.so -call-black-box \
    -call-black-box-fn $BB_NAME -call-black-box-top $TOP_NAME - -o $LLVM_OUTPUT
else 
  $LLVM_OPT -mem2reg -load \
  $LLVM_PASS_DIR/LLVMCustomPasses.so -call-black-box \
  -call-black-box-fn $BB_NAME $LLVM_INPUT -o $LLVM_OUTPUT
fi

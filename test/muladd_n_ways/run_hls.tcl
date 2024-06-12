source ../../scripts/SILVIA.tcl
set SILVIA::ROOT "../.."
set SILVIA::LLVM_ROOT "${SILVIA::ROOT}/llvm-project/install"
set SILVIA::PASSES [list [dict create OP "muladd" MUL_ONLY 1 OP_SIZE 4 INLINE 0]]
set SILVIA::DEBUG 1

delete_project proj
open_project proj
open_solution sol
add_files main.cpp
add_files -tb main.cpp
set_top muladd_dut
set_part xck26-sfvc784-2LV-c
SILVIA::csynth_design
cosim_design

exit
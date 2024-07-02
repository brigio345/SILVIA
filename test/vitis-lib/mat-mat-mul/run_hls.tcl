set SILVIA_ROOT ../../..
source ${SILVIA_ROOT}/scripts/SILVIA.tcl
set SILVIA::ROOT ${SILVIA_ROOT}
set SILVIA::LLVM_ROOT ${SILVIA::ROOT}/llvm-project/install
set SILVIA::DEBUG 1
set SILVIA::PASSES [list [dict create OP "muladd" INLINE 0 MAX_CHAIN_LEN 3] ]
  # [dict create OP "add" FACTOR 2]]

set PROJ_NAME "proj"
set FPGA_DEVICE xck26-sfvc784-2LV-c

set BOOL_INPUTS_CFLAG [expr {[info exists ::env(USE_BOOL_INPUTS)] ? "BOOL_INPUTS" : "NO_BOOL_INPUTS"}]

open_project -reset ${PROJ_NAME}
add_files dut.cpp -cflags "-I../include -DDEBUG -D${BOOL_INPUTS_CFLAG}"
add_files -tb main.cpp -cflags "-D${BOOL_INPUTS_CFLAG}"
set_top dut
open_solution -reset solution1
set_part $FPGA_DEVICE
create_clock -period "250MHz"

# csim_design
SILVIA::csynth_design 
# cosim_design
export_design -flow syn -rtl verilog
close_solution 
open_solution solution1_FE
export_design -flow syn -rtl verilog

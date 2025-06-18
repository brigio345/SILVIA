set SILVIA_ROOT $::env(SILVIA_ROOT)
source ${SILVIA_ROOT}/scripts/SILVIA.tcl
set SILVIA::ROOT ${SILVIA_ROOT}
set SILVIA::LLVM_ROOT ${SILVIA::ROOT}/llvm-project/install
set SILVIA::DEBUG 1
set SILVIA::PASSES [list [dict create OP "muladd" OP_SIZE 4] [dict create OP "muladd" OP_SIZE 8 INLINE 0 MAX_CHAIN_LEN 3]]
  # [dict create OP "add" FACTOR 2]]

set PROJ_NAME "proj"
set FPGA_DEVICE xck26-sfvc784-2LV-c

set BOOL_INPUTS_CFLAG [expr {[info exists ::env(USE_BOOL_INPUTS)] ? "BOOL_INPUTS" : "NO_BOOL_INPUTS"}]

set freq 250

open_project proj

add_files src/dut.cpp -cflags "-Iinclude -DDEBUG -D${BOOL_INPUTS_CFLAG}"
add_files -tb src/main.cpp -cflags "-Iinclude -D${BOOL_INPUTS_CFLAG}"

set_top dut

open_solution -reset sol_${freq}mhz

set_part $FPGA_DEVICE

create_clock -period "${freq}MHz"

# exit

SILVIA::csynth_design 
#csynth_design
#cosim_design
#export_design -flow impl -rtl verilog
#close_solution 
#open_solution solution1_FE
#export_design -flow impl -rtl verilog
#cosim_design -trace_level all -wave_debug -setup
exit

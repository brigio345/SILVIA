set SILVIA_ROOT $::env(SILVIA_ROOT)
source ${SILVIA_ROOT}/scripts/SILVIA.tcl
set SILVIA::ROOT ${SILVIA_ROOT}
set SILVIA::LLVM_ROOT ${SILVIA::ROOT}/llvm-project/install
set SILVIA::DEBUG 1
set SILVIA::PASSES [list [dict create OP "muladd" MUL_ONLY 0 INLINE 1] ]
# set SILVIA::PASSES [list [dict create OP "add" FACTOR 4] [dict create OP "add" FACTOR 4]]
  # [dict create OP "add" FACTOR 2]]

set freq 250

set FPGA_DEVICE xck26-sfvc784-2LV-c

set BOOL_INPUTS_CFLAG [expr {[info exists ::env(USE_BOOL_INPUTS)] ? "BOOL_INPUTS" : "NO_BOOL_INPUTS"}]

open_project proj

add_files src/main.cpp -cflags "-Iinclude -DDEBUG -D${BOOL_INPUTS_CFLAG}"
# add_files -tb main.cpp -cflags "-D${BOOL_INPUTS_CFLAG}"

set_top dut

open_solution sol_${freq}mhz

set_part $FPGA_DEVICE

create_clock -period "${freq}MHz"

# exit

# csim_design
config_op -impl dsp mul
SILVIA::csynth_design
# cosim_design
#export_design -flow impl -rtl verilog
#close_solution 
#open_solution solution1_FE
#export_design -flow impl -rtl verilog
# cosim_design

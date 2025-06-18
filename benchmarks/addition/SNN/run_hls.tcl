set SILVIA_ROOT $::env(SILVIA_ROOT)
source ${SILVIA_ROOT}/scripts/SILVIA.tcl
set SILVIA::ROOT ${SILVIA_ROOT}
set SILVIA::LLVM_ROOT ${SILVIA::ROOT}/llvm-project/install
set SILVIA::DEBUG 1
set SILVIA::PASSES [list [dict create OP "add" OP_SIZE 12]]

set FPGA_DEVICE xck26-sfvc784-2LV-c

set STRANGE_MODULO "NO_STRANGE_MODULO"
set USE_BRRR "NO_USE_BRRR"

set freq 400

open_project prj
add_files -cflags "-Iinclude -D${STRANGE_MODULO} -D${USE_BRRR}" "src/dut.cc"
add_files -tb -cflags "-Iinclude" "src/tb.cc"
open_solution -reset sol_${freq}mhz 
set_part $FPGA_DEVICE
create_clock -period "${freq}MHz" -name default
set_top core
#config_op -impl dsp add
#csim_design
if {$USE_BRRR == "USE_BRRR"} {
  csynth_design
} else {
  SILVIA::csynth_design
}
#exit
export_design -flow impl -rtl verilog
#close_solution
#open_solution solution1_FE
#export_design -flow impl -rtl verilog
exit

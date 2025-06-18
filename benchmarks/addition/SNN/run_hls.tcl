set SILVIA_ROOT $::env(SILVIA_ROOT)
source ${SILVIA_ROOT}/scripts/SILVIA.tcl
set SILVIA::ROOT ${SILVIA_ROOT}
set SILVIA::LLVM_ROOT ${SILVIA::ROOT}/llvm-project/install
set SILVIA::DEBUG 1
set SILVIA::PASSES [list [dict create OP "add" OP_SIZE 12] [dict create OP "add" OP_SIZE 24]]

set FPGA_DEVICE xck26-sfvc784-2LV-c

set VERSION "BD"
#set VERSION "BU"
#set VERSION "S"
set freq 250

open_project proj
add_files -cflags "-Iinclude -DNO_STRANGE_MODULO -DNO_USE_BRRR" "src/dut.cc"
add_files -tb -cflags "-Iinclude" "src/tb.cc"
open_solution -reset sol_${VERSION}_${freq}mhz_${VERSION}
set_part $FPGA_DEVICE
create_clock -period "${freq}MHz"
set_top core
if {${VERSION} == "BU"} {
	config_op add -latency 1
} else {
	config_op add -impl dsp -latency 1
}

#csim_design
if {${VERSION} == "S"} {
	SILVIA::csynth_design
} else {
	csynth_design
}
#cosim_design
export_design -flow impl -rtl verilog

exit

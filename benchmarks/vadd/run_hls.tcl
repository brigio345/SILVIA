set freq 400
#delete_project project_${freq}_new
open_project proj
set_top vadd
add_files src/vadd.cpp -cflags "-Wall"
add_files -tb src/tb.cpp -cflags "-Wno-unknown-pragmas" -csimflags "-Wno-unknown-pragmas"
open_solution -reset sol_${freq}mhz_nobind -flow_target vitis
set_part "xck26-sfvc784-2LV-c"
#config_op add -impl dsp
create_clock -period "${freq} MHz" -name default
set SILVIA_ROOT $::env(SILVIA_ROOT)
source ${SILVIA_ROOT}/scripts/SILVIA.tcl
set SILVIA::ROOT ${SILVIA_ROOT}
set SILVIA::LLVM_ROOT ${SILVIA_ROOT}/llvm-project/install
set SILVIA::DEBUG 1
set SILVIA::PASSES [list \
	[dict create OP "add" OP_SIZE 12] \
	[dict create OP "add" OP_SIZE 24]]
#csim_design
SILVIA::csynth_design
#csynth_design
export_design -flow impl
#cosim_design
exit

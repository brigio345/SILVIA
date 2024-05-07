set SILVIA_ROOT "../.."
source ${SILVIA_ROOT}/scripts/SILVIA.tcl
set SILVIA::ROOT ${SILVIA_ROOT}
set SILVIA::LLVM_ROOT ${SILVIA_ROOT}/llvm-project/install
set SILVIA::DEBUG 1
set SILVIA::PASSES [list \
	[dict create OP "add" FACTOR 4] \
	[dict create OP "add" FACTOR 2]]

set PROJ_NAME "proj"

open_project -reset ${PROJ_NAME}

add_files main.cpp
add_files -tb main.cpp

set_top matrix_multiplication_wino

open_solution -reset solution1

set_part "xczu3eg-ubva530-2L-e"

create_clock -period "300MHz"

csim_design
SILVIA::csynth_design
cosim_design

exit

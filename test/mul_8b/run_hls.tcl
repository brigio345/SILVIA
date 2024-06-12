set SILVIA_ROOT "../.."
source ${SILVIA_ROOT}/scripts/SILVIA.tcl
set SILVIA::ROOT ${SILVIA_ROOT}
set SILVIA::LLVM_ROOT ${SILVIA_ROOT}/llvm-project/install
set SILVIA::DEBUG 1
set SILVIA::PASSES [list [dict create OP "muladd"]]

set PROJ_NAME "proj"

delete_project ${PROJ_NAME}
open_project ${PROJ_NAME}

add_files main.cpp
add_files -tb main.cpp

set_top mul_8b

open_solution solution1

set_part "xczu3eg-ubva530-2L-e"

create_clock -period "300MHz"

SILVIA::csynth_design
cosim_design

exit

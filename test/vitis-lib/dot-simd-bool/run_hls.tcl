set SILVIA_ROOT "../../.."
source ${SILVIA_ROOT}/scripts/SILVIA.tcl
set SILVIA::ROOT ${SILVIA_ROOT}
set SILVIA::LLVM_ROOT ${SILVIA_ROOT}/llvm-project/install
set SILVIA::DEBUG 1

set PROJ_NAME "proj"

set BOOL_INPUTS_CFLAG [expr {[info exists ::env(USE_BOOL_INPUTS)] ? "BOOL_INPUTS" : "NO_BOOL_INPUTS"}]

open_project -reset ${PROJ_NAME}

add_files dut.cpp -cflags "-I../include -DDEBUG -D${BOOL_INPUTS_CFLAG}"
add_files -tb main.cpp -cflags "-D${BOOL_INPUTS_CFLAG}"

set_top dut

open_solution -reset solution1

set_part "xczu3eg-ubva530-2L-e"

create_clock -period "300MHz"

csim_design
# csynth_design
if {[info exists ::env(USE_BOOL_INPUTS)]} {
set SILVIA::PASSES [list [dict create OP "add" FACTOR 2]]
} else {
set SILVIA::PASSES [list [dict create OP "muladd"]]
}
SILVIA::csynth_design
cosim_design

exit

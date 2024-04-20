source ../../../scripts/csynth_design_simd.tcl

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
csynth_design_simd "add" 2 ../../../llvm-3.1/llvm/install ../../..
} else {
csynth_design_simd "muladd" 2 ../../../llvm-3.1/llvm/install ../../..
}
# cosim_design

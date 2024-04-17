source ../../../scripts/csynth_design_simd.tcl

set PROJ_NAME "proj"

open_project -reset ${PROJ_NAME}

add_files dut.cpp -cflags "-I../include -DDEBUG"
add_files -tb main.cpp

set_top dut

open_solution -reset solution1

set_part "xczu3eg-ubva530-2L-e"

create_clock -period "300MHz"

csim_design
# csynth_design
csynth_design_simd "muladd" 2 ${PROJ_NAME}/solution1 ../../../llvm-3.1/llvm/install ../../..
# cosim_design

exit

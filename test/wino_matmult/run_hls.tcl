source ../../scripts/csynth_design_simd.tcl

set PROJ_NAME "proj"

open_project -reset ${PROJ_NAME}

add_files main.cpp
add_files -tb main.cpp

set_top matrix_multiplication_wino

open_solution -reset solution1

set_part "xczu3eg-ubva530-2L-e"

create_clock -period "300MHz"

csim_design
csynth_design_simd "add" 4 ../../llvm-3.1/llvm/install ../..
cosim_design

exit

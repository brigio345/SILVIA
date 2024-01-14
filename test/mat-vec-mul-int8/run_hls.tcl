# source ../../scripts/csynth_design_simd.tcl

set PROJ_NAME "proj"
set VERSAL "xcvc1902-vsva2197-2MP-e-S"

open_project -reset ${PROJ_NAME}

add_files test.cpp
add_files -tb test.cpp
# add_files -blackbox blackbox/dotprod/dotprod.json

set_top example

open_solution -reset solution1

# set_part "xczu3eg-ubva530-2L-e"
set_part $VERSAL

create_clock -period "300MHz"

# csynth_design_simd dotprod ${PROJ_NAME}/solution1 ../../llvm-3.1/llvm/install ../..
csynth_design 

cosim_design

exit

source ../../scripts/csynth_design_simd.tcl

set PROJ_NAME "proj_whole_flow"

open_project -reset ${PROJ_NAME}

add_files test.cpp
add_files -tb test.cpp

set_top example

open_solution -reset solution1

set_part "xczu3eg-ubva530-2L-e"

create_clock -period "300MHz"
if { ![info exists ::env(HLS_LLVM_PLUGIN_DIR)] } {
    # Use plugin example directory as default build directory
  set ::env(HLS_LLVM_PLUGIN_DIR) [file normalize ../../llvm-3.9/lib/InsertDummyBB]
}
csynth_design_simd ${PROJ_NAME}/solution1 \
  $::env(HOME)/llvm/hls-llvm-simd/llvm-3.1/llvm/install ../..

cosim_design

exit

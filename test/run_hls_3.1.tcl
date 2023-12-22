::common::set_param hls.enable_hidden_option_error false

open_project -reset proj

# Add kernel and testbench
add_files hls_example.cpp
add_files -tb hls_example.cpp
add_files -blackbox ../blackbox/dsp_add_4simd_pipe_l0.json

# Tell the top
set_top example

# Open a solution and remove any existing data
open_solution -reset solution1

# Set the target device
set_part "xczu3eg-ubva530-2L-e"

# Create a virtual clock for the current solution
create_clock -period "300MHz"

# Compile and runs pre-synthesis C simulation using the provided C test bench
#csim_design

# Synthesize to RTL
csynth_design

# Execute post-synthesis co-simulation of the synthesized RTL with the original C/C++-based test bench
#cosim_design

exec unzip -o -d dut proj/solution1/.autopilot/db/dut.hcp
set ::env(LD_LIBRARY_PATH) "$::env(HOME)/hls-llvm-simd/llvm/install/lib/:$::env(LD_LIBRARY_PATH)"
exec $::env(HOME)/hls-llvm-simd/llvm/install/bin/opt -load $::env(HOME)/hls-llvm-simd/llvm/install/lib/LLVMSIMDAdd.so -simd-add dut/a.o.3.bc -o dut/a.o.3.bc |& cat | tee SIMDAdd.log
exec zip -rj dut.hcp dut
read_checkpoint dut.hcp
csynth_design -hw_syn
cosim_design
#export_design -flow syn

#exit


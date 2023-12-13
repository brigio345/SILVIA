set USE_BB 1
# Open a project and remove any existing data
open_project -reset proj_vitis

# Add kernel and testbench
add_files main.cpp
if {$USE_BB == 1} {
  add_files -blackbox doubleMulAdd.json
}
add_files -tb main.cpp

# Tell the top
set_top dut

# Open a solution and remove any existing data
open_solution -reset solution1

# Set the target device
set_part xck26-sfvc784-2LV-c

# Create a virtual clock for the current solution
create_clock -period "300MHz"

# Compile and runs pre-synthesis C simulation using the provided C test bench
csim_design

# Synthesize to RTL
csynth_design
if {$USE_BB == 1} {
  exec make prepare_cosim 
}
cosim_design
if {$USE_BB == 1} {
  exec make prepare_export 
}
export_design -rtl verilog

# Execute post-synthesis co-simulation of the synthesized RTL with the original C/C++-based test bench
# cosim_design

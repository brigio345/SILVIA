set freq 250
open_project proj
set_top GAT_compute_graphs
add_files -cflags "-Iinclude" src/GAT_compute.cc
add_files -cflags "-Iinclude" src/conv_layer.cc
add_files -cflags "-Iinclude" src/finalize.cc
add_files -cflags "-Iinclude" src/globals.cc
add_files -cflags "-Iinclude" src/linear.cc
add_files -cflags "-Iinclude" src/load_inputs.cc
add_files -cflags "-Iinclude" src/message_passing.cc
add_files -cflags "-Iinclude" src/node_embedding.cc
add_files -tb testbench/main.cc -cflags "-Iinclude -Wno-unknown-pragmas" -csimflags "-Wno-unknown-pragmas"
add_files -tb testbench/load.cc -cflags "-Iinclude -Wno-unknown-pragmas" -csimflags "-Wno-unknown-pragmas"
add_files -tb g1_node_feature.bin -cflags "-Iinclude -Wno-unknown-pragmas" -csimflags "-Wno-unknown-pragmas"
add_files -tb g1_info.txt -cflags "-Iinclude -Wno-unknown-pragmas" -csimflags "-Wno-unknown-pragmas"
add_files -tb g1_edge_list.bin -cflags "-Iinclude -Wno-unknown-pragmas" -csimflags "-Wno-unknown-pragmas"
add_files -tb g1_edge_attr.bin -cflags "-Iinclude -Wno-unknown-pragmas" -csimflags "-Wno-unknown-pragmas"
add_files -tb gat_ep1_skip_proj_weight_1_layer5.bin -cflags "-Iinclude -Wno-unknown-pragmas" -csimflags "-Wno-unknown-pragmas"
add_files -tb gat_ep1_skip_proj_weight_0_layer5.bin -cflags "-Iinclude -Wno-unknown-pragmas" -csimflags "-Wno-unknown-pragmas"
add_files -tb gat_ep1_scoring_fn_target_layer5.bin -cflags "-Iinclude -Wno-unknown-pragmas" -csimflags "-Wno-unknown-pragmas"
add_files -tb gat_ep1_scoring_fn_source_layer5.bin -cflags "-Iinclude -Wno-unknown-pragmas" -csimflags "-Wno-unknown-pragmas"
add_files -tb gat_ep1_pred_weights_layer5.bin -cflags "-Iinclude -Wno-unknown-pragmas" -csimflags "-Wno-unknown-pragmas"
add_files -tb gat_ep1_pred_bias_layer5.bin -cflags "-Iinclude -Wno-unknown-pragmas" -csimflags "-Wno-unknown-pragmas"
add_files -tb gat_ep1_linear_proj_weight_1_layer5.bin -cflags "-Iinclude -Wno-unknown-pragmas" -csimflags "-Wno-unknown-pragmas"
add_files -tb gat_ep1_linear_proj_weight_0_layer5.bin -cflags "-Iinclude -Wno-unknown-pragmas" -csimflags "-Wno-unknown-pragmas"
add_files -tb gat_ep1_layer5.weights.all.bin -cflags "-Iinclude -Wno-unknown-pragmas" -csimflags "-Wno-unknown-pragmas"
open_solution sol_${freq}mhz -flow_target vitis
#set_part "xck26-sfvc784-2LV-c"
set_part "xczu9eg-ffvb1156-2-e"
#set_part "xcu50-fsvh2104-2-e"
create_clock -period ${freq}MHz -name default
config_op mul -impl dsp
## C code synthesis to generate Verilog code
#set SILVIA_ROOT "/home-ssd/brignone/gandalf/llvm/hls-llvm-pass"
set SILVIA_ROOT "/home/brignone/llvm/hls-llvm-pass"
source ${SILVIA_ROOT}/scripts/SILVIA.tcl
set SILVIA::ROOT ${SILVIA_ROOT}
set SILVIA::LLVM_ROOT ${SILVIA_ROOT}/llvm-project/install
set SILVIA::DEBUG 1
set SILVIA::PASSES [list [dict create OP "muladd" INLINE 1 MAX_CHAIN_LEN 3] [dict create OP "muladd" MUL_ONLY 1 INLINE 1]]
SILVIA::csynth_design
#export_design -flow impl
exit

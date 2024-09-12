set freq 250
open_project proj
set_top Gsm_LPC_Analysis
add_files -cflags "-Iinclude" src/add.c
add_files -cflags "-Iinclude" src/lpc.c
add_files -cflags "-Iinclude" include/private.h
open_solution -reset sol_${freq}mhz
set_part "xck26-sfvc784-2LV-c"
create_clock -period "${freq}MHz" -name default
config_rtl -reset state
#source opt.tcl
config_op mul -impl dsp
## C code synthesis to generate Verilog code
set SILVIA_ROOT $::env(SILVIA_ROOT)
source ${SILVIA_ROOT}/scripts/SILVIA.tcl
set SILVIA::ROOT ${SILVIA_ROOT}
set SILVIA::LLVM_ROOT ${SILVIA_ROOT}/llvm-project/install
set SILVIA::DEBUG 1
set SILVIA::PASSES [list [dict create OP "muladd" INLINE 1 MAX_CHAIN_LEN 3] [dict create OP "muladd" INLINE 1 MUL_ONLY 1]]
SILVIA::csynth_design
#export_design -flow impl
exit

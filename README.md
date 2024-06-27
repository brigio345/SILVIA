## Overview:
SILVIA is a set of LLVM transformation passes to automatically identify superword-level parallelism within an HLS FPGA design and exploits it by packing multiple operations, such as additions, multiplications, and multiply-and-adds, into a single DSP.

## Build:
Build and install LLVM 3.1:
```bash
source install_llvm.sh
```
Build and install the SILVIA LLVM passes:
```bash
source build_pass.sh
```

## Use:
Update a standard Vitis HLS build script according to:
```diff
+ source ${SILVIA_ROOT}/scripts/SILVIA.tcl
open_project ${PROJ_NAME}
open_solution ${SOL_NAME}
add_files ${SOURCE_FILES}
set_top ${TOP_NAME}
- csynth_design
+ set SILVIA::PASSES \
+ [list [dict create OP "muladd"] \
+ [dict create OP "add" OP_SIZE 12]]
+ SILVIA::csynth_design
export_design
```

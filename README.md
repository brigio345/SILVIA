SILVIA is a set of LLVM transformation passes to automatically identify superword-level parallelism within an HLS FPGA design and exploit it by packing multiple operations, such as additions, multiplications, and multiply-and-adds, into a single DSP.
It currently supports AMD Vitis HLS.

## Prerequisites
Install [AMD Vitis](https://www.xilinx.com/support/download/index.html/content/xilinx/en/downloadNav/vitis.html) and set it up via:
```bash
source ${INSTALL_PATH}/Vitis/${VERSION}/settings.sh
```
Where `${INSTALL_PATH}` is the installation path of Vitis (e.g., `/opt/Xilinx`) and `${VERSION}` is the version of Vitis (e.g., `2024.1`).

## Build & Install
Build and install LLVM 3.1:
```bash
source install_llvm.sh
```
Build and install the SILVIA LLVM passes:
```bash
source build_pass.sh
```

## Run
Update the Vitis HLS TCL synthesis script by:
1. Importing the SILVIA APIs with: `source ${SILVIA_ROOT}/scripts/SILVIA.tcl`, where `${SILVIA_ROOT}` points to the root of the SILVIA repository (e.g., `/home/user/SILVIA`).
2. Populating `SILVIA::PASSES` with the list of passes to run.
   Each pass is specified as a `dict` with a mandatory `OP` key, which selects the pass (`"add"` or `"muladd"`).
   Optional keys include:
   * `OP_SIZE`, which defines the maximum size of the operands to pack (`12` or `24` for `"add"`, `4` or `8` for `"muladd"`).
   * `INST`, which defines the instruction to pack (`"add"` or `"sub"` only when `OP` is `"add"`).
   * `MAX_CHAIN_LEN`, which defines an upper limit to the number of cascaded DSPs (only when `OP` is `"muladd"`).
4. Replacing the command `csynth_design` with the custom `SILVIA::csynth_design`.

The following snippet shows the changes to a synthesis script to run the `"muladd"` pass and the `"add"` pass with operands of up to 12 bits:
```diff
- csynth_design
+ source ${SILVIA_ROOT}/scripts/SILVIA.tcl
+ set SILVIA::PASSES \
+ [list [dict create OP "muladd"] \
+ [dict create OP "add" OP_SIZE 12]]
+ SILVIA::csynth_design
```

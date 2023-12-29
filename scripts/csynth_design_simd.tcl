::common::set_param hls.enable_hidden_option_error false

proc csynth_design_simd {solution_path llvm_install_path {blackbox_root ".."}} {
	if { ![file exists $::env(HLS_LLVM_PLUGIN_DIR)/LLVMCustomPasses.so] } {
		error "Must build LLVMCustomPasses.so before running this example"
	}

	set ::LLVM_CUSTOM_LINK \
		$::env(XILINX_HLS)/lnx64/tools/clang-3.9-csynth/bin/llvm-link
	set ::LLVM_CUSTOM_NM \
		$::env(XILINX_HLS)/lnx64/tools/clang-3.9-csynth/bin/llvm-nm
	set ::BB_BC ${solution_path}/.autopilot/db/dsp_add_simd.g.bc

	set ::LLVM_CUSTOM_CMD {$::env(HOME)/llvm/hls-llvm-pass/scripts/safe_link.sh \
		$::LLVM_CUSTOM_NM $::LLVM_CUSTOM_LINK $LLVM_CUSTOM_OPT \
		$LLVM_CUSTOM_INPUT $LLVM_CUSTOM_OUTPUT $::env(HLS_LLVM_PLUGIN_DIR) $::BB_BC\
	}

	exec cp -r ${blackbox_root}/blackbox .
	add_files -blackbox blackbox/dsp_add_4simd_pipe_l0.json
	csynth_design
	exec unzip -o -d dut ${solution_path}/.autopilot/db/dut.hcp
	set ::env(LD_LIBRARY_PATH) "${llvm_install_path}/lib/:$::env(LD_LIBRARY_PATH)"
	exec ${llvm_install_path}/bin/opt -load ${llvm_install_path}/lib/LLVMSIMDAdd.so -simd-add dut/a.o.3.bc -o dut/a.o.3.bc |& cat | tee SIMDAdd.log
	exec zip -rj dut.hcp dut
	read_checkpoint dut.hcp
	csynth_design -hw_syn
}

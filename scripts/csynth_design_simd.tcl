::common::set_param hls.enable_hidden_option_error false

proc csynth_design_simd {bb_name top_name solution_path llvm_install_path {simd_root ".."}} {
	if { ![info exists ::env(HLS_LLVM_PLUGIN_DIR)] } {
		set ::env(HLS_LLVM_PLUGIN_DIR) [file normalize ../../llvm-3.9/lib/CallBlackBox]
	}

	if { ![file exists $::env(HLS_LLVM_PLUGIN_DIR)/LLVMCustomPasses.so] } {
		error "Must build LLVMCustomPasses.so before running this example"
	}

	set ::SIMD_ROOT ${simd_root}
	set ::LLVM_CUSTOM_LINK \
		$::env(XILINX_HLS)/lnx64/tools/clang-3.9-csynth/bin/llvm-link
	set ::LLVM_CUSTOM_NM \
		$::env(XILINX_HLS)/lnx64/tools/clang-3.9-csynth/bin/llvm-nm
	set ::SOLUTION_DIR $solution_path
	set ::BB_NAME $bb_name
	set ::TOP_NAME $top_name

	set ::LLVM_CUSTOM_CMD {$::SIMD_ROOT/scripts/safe_link.sh \
		$::LLVM_CUSTOM_NM $::LLVM_CUSTOM_LINK $LLVM_CUSTOM_OPT \
		$LLVM_CUSTOM_INPUT $LLVM_CUSTOM_OUTPUT \
		$::env(HLS_LLVM_PLUGIN_DIR) $::SOLUTION_DIR $::BB_NAME \
		$::TOP_NAME
	}

	exec ln -sf ${simd_root}/blackbox .
	add_files -blackbox blackbox/$bb_name/$bb_name.json
	csynth_design
	exec unzip -o -d dut ${solution_path}/.autopilot/db/dut.hcp
	set ::env(LD_LIBRARY_PATH) "${llvm_install_path}/lib/:$::env(LD_LIBRARY_PATH)"
	switch $bb_name {
		"add4simd" {exec ${llvm_install_path}/bin/opt -load ${llvm_install_path}/lib/LLVMSIMDAdd.so -simd-add -dce dut/a.o.3.bc -o dut/a.o.3.bc |& cat | tee SIMDAdd.log}
		"dotprod" {exec ${llvm_install_path}/bin/opt -load ${llvm_install_path}/lib/LLVMDotProdize.so -dot-prod-ize -dce dut/a.o.3.bc -o dut/a.o.3.bc |& cat | tee DotProdize.log}
		default {puts "bb_name=$bb_name is invalid."}
	}
	exec zip -rj dut.hcp dut
	read_checkpoint dut.hcp
	csynth_design -hw_syn
}

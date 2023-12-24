::common::set_param hls.enable_hidden_option_error false

proc csynth_design_simd {solution_path llvm_install_path} {
	# TODO: call the pass inserting the dummy blackbox call
	#set ::LLVM_CUSTOM_CMD {$LLVM_CUSTOM_OPT -load $::env(HLS_LLVM_PLUGIN_DIR)/LLVMCustomPasses.so -mypass $LLVM_CUSTOM_INPUT -o $LLVM_CUSTOM_OUTPUT}
	add_files -blackbox ../blackbox/dsp_add_4simd_pipe_l0.json
	csynth_design
	exec unzip -o -d dut ${solution_path}/.autopilot/db/dut.hcp
	set ::env(LD_LIBRARY_PATH) "${llvm_install_path}/lib/:$::env(LD_LIBRARY_PATH)"
	exec ${llvm_install_path}/bin/opt -load ${llvm_install_path}/lib/LLVMSIMDAdd.so -simd-add dut/a.o.3.bc -o dut/a.o.3.bc |& cat | tee SIMDAdd.log
	exec zip -rj dut.hcp dut
	read_checkpoint dut.hcp
	csynth_design -hw_syn
}

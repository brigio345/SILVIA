::common::set_param hls.enable_hidden_option_error false

proc csynth_design_simd {simd_op simd_factor solution_path llvm_install_path {simd_root ".."}} {
	if { ![info exists ::env(HLS_LLVM_PLUGIN_DIR)] } {
		set ::env(HLS_LLVM_PLUGIN_DIR) [file normalize ../../llvm-3.9/lib/CallBlackBox]
	}

	if { ![file exists $::env(HLS_LLVM_PLUGIN_DIR)/LLVMCustomPasses.so] } {
		error "Must build LLVMCustomPasses.so before running this example"
	}

	csynth_design
	exec unzip -o -d dut ${solution_path}/.autopilot/db/dut.hcp
	set ::env(LD_LIBRARY_PATH) "${llvm_install_path}/lib/:$::env(LD_LIBRARY_PATH)"
	exec ${llvm_install_path}/bin/llvm-link ${simd_root}/template/${simd_op}/${simd_op}.ll dut/a.o.3.bc -o dut/a.o.3.bc
	exec ${llvm_install_path}/bin/opt -load ${llvm_install_path}/lib/LLVMSIMDAdd.so -basicaa -simd-add -simd-add-op ${simd_op} -simd-add-factor ${simd_factor} -dce dut/a.o.3.bc -o dut/a.o.3.bc |& cat | tee SIMDAdd.log
	exec zip -rj dut.hcp dut
	read_checkpoint dut.hcp
	csynth_design -hw_syn

	if { ${simd_op} == "add" } {
		foreach dir "syn impl" {
			set simd_name "_simd_${simd_op}_${simd_factor}"
			foreach f [glob -nocomplain ${solution_path}/${dir}/verilog/*${simd_name}.v] {
				set fbasename [file tail $f]
				if {[regexp "^(.*)${simd_name}\.v\$" $fbasename -> prefix]} {
					file copy -force ${simd_root}/template/${simd_op}/${simd_op}.v $f
					exec sh -c "sed -i 's/${simd_name}/${prefix}${simd_name}/g' $f"
				}
			}
		}
	}
}

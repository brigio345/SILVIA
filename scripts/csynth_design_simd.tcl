::common::set_param hls.enable_hidden_option_error false

proc csynth_design_simd {bb_name solution_path llvm_install_path {simd_root ".."}} {
	if { ![info exists ::env(HLS_LLVM_PLUGIN_DIR)] } {
		set ::env(HLS_LLVM_PLUGIN_DIR) [file normalize ../../llvm-3.9/lib/CallBlackBox]
	}

	if { ![file exists $::env(HLS_LLVM_PLUGIN_DIR)/LLVMCustomPasses.so] } {
		error "Must build LLVMCustomPasses.so before running this example"
	}

	csynth_design
	exec unzip -o -d dut ${solution_path}/.autopilot/db/dut.hcp
	set ::env(LD_LIBRARY_PATH) "${llvm_install_path}/lib/:$::env(LD_LIBRARY_PATH)"
	exec ${llvm_install_path}/bin/llvm-as ${simd_root}/template/${bb_name}/${bb_name}.ll -o dut/${bb_name}.bc
	exec ${llvm_install_path}/bin/llvm-link dut/${bb_name}.bc dut/a.o.3.bc -o dut/a.o.3.bc
	switch $bb_name {
		"add4simd" {exec ${llvm_install_path}/bin/opt -load ${llvm_install_path}/lib/LLVMSIMDAdd.so -simd-add -dce dut/a.o.3.bc -o dut/a.o.3.bc |& cat | tee SIMDAdd.log}
		"dotprod" {exec ${llvm_install_path}/bin/opt -load ${llvm_install_path}/lib/LLVMDotProdize.so -dot-prod-ize -dce dut/a.o.3.bc -o dut/a.o.3.bc |& cat | tee DotProdize.log}
		default {puts "bb_name=$bb_name is invalid."}
	}
	exec zip -rj dut.hcp dut
	read_checkpoint dut.hcp
	csynth_design -hw_syn

	foreach dir "syn impl" {
		foreach f [glob -nocomplain ${solution_path}/${dir}/verilog/*_${bb_name}.v] {
			set fbasename [file tail $f]
			if {[regexp "^(.*)_${bb_name}\.v\$" $fbasename -> prefix]} {
				file copy -force ${simd_root}/template/${bb_name}/${bb_name}.v $f
				exec sh -c "sed -i 's/${bb_name}/${prefix}_${bb_name}/g' $f"
			}
		}
	}
}

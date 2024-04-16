package require tdom

::common::set_param hls.enable_hidden_option_error false

proc csynth_design_simd {simd_op simd_factor llvm_install_path {simd_root ".."}} {
	set project_path [get_project -directory]
	set solution_name [get_solution]
	file copy -force -- ${project_path}/${solution_name} ${project_path}/${solution_name}_FE
	file rename ${project_path}/${solution_name}_FE/${solution_name}.aps ${project_path}/${solution_name}_FE/${solution_name}_FE.aps
	set aps_file [open ${project_path}/${solution_name}_FE/${solution_name}_FE.aps r]
	set doc [dom parse [read ${aps_file}]]
	close ${aps_file}
	set root [${doc} documentElement]
	set solution_name_node [${root} selectNode "/AutoPilot:solution/name/value"]
	${solution_name_node} setAttribute string ${solution_name}_FE
	set aps_file [open ${project_path}/${solution_name}_FE/${solution_name}_FE.aps w]
	puts ${aps_file} [${doc} asXML]
	close ${aps_file}
	open_solution ${solution_name}_FE
	csynth_design
	exec unzip -o -d dut ${project_path}/${solution_name}_FE/.autopilot/db/dut.hcp
	set ::env(LD_LIBRARY_PATH) "${llvm_install_path}/lib/:$::env(LD_LIBRARY_PATH)"
	exec ${llvm_install_path}/bin/llvm-link ${simd_root}/template/${simd_op}/${simd_op}.ll dut/a.o.3.bc -o dut/a.o.3.bc
	exec ${llvm_install_path}/bin/opt -load ${llvm_install_path}/lib/LLVMSILVIA.so -basicaa -silvia -silvia-op ${simd_op} -silvia-simd-factor ${simd_factor} -dce dut/a.o.3.bc -o dut/a.o.3.bc |& cat | tee SILVIA.log
	exec zip -rj dut.hcp dut
	open_solution ${solution_name}
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

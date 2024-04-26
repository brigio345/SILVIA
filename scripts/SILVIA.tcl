package require tdom

::common::set_param hls.enable_hidden_option_error false

namespace eval SILVIA {
	variable ROOT
	variable LLVM_ROOT
	variable SIMD_OP

	proc csynth_design {} {
		variable ROOT
		variable LLVM_ROOT
		variable SIMD_OP

		set simd_op [lindex [dict keys ${SIMD_OP}] 0]
		set simd_factor [dict get ${SIMD_OP} ${simd_op}]

		set project_path [get_project -directory]
		set solution_name [get_solution]
		if { [file exists ${project_path}/${solution_name}_FE] } {
		  file delete -force -- ${project_path}/${solution_name}_FE
		}
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
		::csynth_design
		exec unzip -o -d dut ${project_path}/${solution_name}_FE/.autopilot/db/dut.hcp
		set ::env(LD_LIBRARY_PATH) "${LLVM_ROOT}/lib/:$::env(LD_LIBRARY_PATH)"
		foreach simd_op [dict keys ${SIMD_OP}] {
			exec ${LLVM_ROOT}/bin/llvm-link ${ROOT}/template/${simd_op}/${simd_op}.ll dut/a.o.3.bc -o dut/a.o.3.bc
			switch ${simd_op} {
				"add" {
					exec ${LLVM_ROOT}/bin/opt -load ${LLVM_ROOT}/lib/LLVMSILVIAAdd.so -basicaa -silvia-add -silvia-add-simd-factor ${simd_factor} -dce dut/a.o.3.bc -o dut/a.o.3.bc |& cat | tee SILVIA.log
				}
				"muladd" {
					exec ${LLVM_ROOT}/bin/opt -load ${LLVM_ROOT}/lib/LLVMSILVIAMuladd.so -basicaa -silvia-muladd -dce dut/a.o.3.bc -o dut/a.o.3.bc |& cat | tee SILVIA.log
				}
			}
		}
		exec zip -rj dut.hcp dut
		open_solution ${solution_name}
		read_checkpoint dut.hcp
		::csynth_design -hw_syn
	
		dict for {simd_op simd_factor} ${SIMD_OP} {
			if { ${simd_op} == "add" } {
				foreach dir "syn impl" {
					set simd_name "_simd_${simd_op}_${simd_factor}"
					foreach f [glob -nocomplain ${project_path}/${solution_name}/${dir}/verilog/*${simd_name}.v] {
						set fbasename [file tail $f]
						if {[regexp "^(.*)${simd_name}\.v\$" $fbasename -> prefix]} {
							file copy -force ${ROOT}/template/${simd_op}/${simd_op}.v $f
							exec sh -c "sed -i 's/${simd_name}/${prefix}${simd_name}/g' $f"
						}
					}
				}
			}
		}
	}
}
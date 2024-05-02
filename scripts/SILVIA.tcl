package require tdom

::common::set_param hls.enable_hidden_option_error false

namespace eval SILVIA {
	variable ROOT
	variable LLVM_ROOT
	variable PASSES
	variable DEBUG 0
	variable DEBUG_FILE "SILVIA.log"

	proc csynth_design {} {
		variable ROOT
		variable LLVM_ROOT
		variable PASSES
		variable DEBUG
		variable DEBUG_FILE

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
		exec vitis_hls -l vitis_hls_FE.log -eval "open_project ${project_path}; open_solution ${solution_name}_FE; csynth_design; exit" &
		while {[file exist ${project_path}/${solution_name}_FE/.autopilot/db/dut.hcp] == 0} {
			after 3000
		}
		exec unzip -o -d dut ${project_path}/${solution_name}_FE/.autopilot/db/dut.hcp
		set ::env(LD_LIBRARY_PATH) "${LLVM_ROOT}/lib/:$::env(LD_LIBRARY_PATH)"
		if {${DEBUG} == 1} {
			file delete ${DEBUG_FILE}
		}
		foreach pass ${PASSES} {
			if {[dict exist ${pass} OP] == 0} {
				puts "Invalid pass due to missing OP: ${pass}"
				continue
			}
			set op [dict get ${pass} OP]
			set factor 2
			if {${op} == "add" && [dict exist ${pass} FACTOR]} {
				set factor [dict get ${pass} FACTOR]
			}
			exec ${LLVM_ROOT}/bin/llvm-link ${ROOT}/template/${op}/${op}.ll dut/a.o.3.bc -o dut/a.o.3.bc
			set opt_cmd "${LLVM_ROOT}/bin/opt"
			if {${DEBUG} == 1} {
				append opt_cmd " -debug"
			}
			append opt_cmd " -load ${LLVM_ROOT}/lib/LLVMSILVIA[string toupper ${op} 0 0].so"
			append opt_cmd " -basicaa -silvia-${op}"
			if {${op} == "add"} {
				append opt_cmd " -silvia-add-simd-factor=${factor}"
			}
			if {(${op} == "mul" || ${op} == "muladd") && [dict exist ${pass} INLINE]} {
				append opt_cmd " -silvia-${op}-inline=[dict get ${pass} INLINE]"
			}
			if {${op} == "muladd" && [dict exist ${pass} MAX_CHAIN_LEN]} {
				append opt_cmd " -silvia-muladd-max-chain-len=[dict get ${pass} MAX_CHAIN_LEN]"
			}
			append opt_cmd " -dce dut/a.o.3.bc -o dut/a.o.3.bc"
			if {${DEBUG} == 1} {
				append opt_cmd " |& cat | tee -a ${DEBUG_FILE}"
			}

			eval exec ${opt_cmd}
		}
		exec zip -rj dut.hcp dut
		open_solution ${solution_name}
		read_checkpoint dut.hcp
		::csynth_design -hw_syn
	
		foreach pass ${PASSES} {
			if {[dict exist ${pass} OP] == 0} {
				continue
			}
			set op [dict get ${pass} OP]
			if {${op} == "add"} {
				set factor 2
				if {[dict exist ${pass} FACTOR]} {
					set factor [dict get ${pass} FACTOR]
				}
				foreach dir "syn impl" {
					set name "_simd_${op}_${factor}"
					foreach f [glob -nocomplain ${project_path}/${solution_name}/${dir}/verilog/*${name}*.v] {
						set fbasename [file tail $f]
						if {[regexp "^(.*)${name}(.*)\.v\$" $fbasename -> prefix suffix]} {
							file copy -force ${ROOT}/template/${op}/${op}.v $f
							exec sh -c "sed -i 's/${name}/${prefix}${name}${suffix}/g' $f"
						}
					}
				}
			}
		}
	}
}

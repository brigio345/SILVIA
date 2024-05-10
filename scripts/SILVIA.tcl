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
		set db_path ${project_path}/${solution_name}/.autopilot/db
		set dut_path ${db_path}/dut
		file mkdir ${dut_path}
		exec unzip -o -d ${dut_path} ${project_path}/${solution_name}_FE/.autopilot/db/dut.hcp
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
			set instruction "add"
			if {[dict exist ${pass} INST]} {
				set instruction [dict get ${pass} INST]
			}
			set factor 2
			if {${op} == "add" && [dict exist ${pass} FACTOR]} {
				set factor [dict get ${pass} FACTOR]
			}
			exec ${LLVM_ROOT}/bin/llvm-link ${ROOT}/template/${op}/${op}.ll ${dut_path}/a.o.3.bc -o ${dut_path}/a.o.3.bc
			set opt_cmd "${LLVM_ROOT}/bin/opt -strip-debug"
			if {${DEBUG} == 1} {
				append opt_cmd " -debug"
			}
			append opt_cmd " -load ${LLVM_ROOT}/lib/LLVMSILVIA[string toupper ${op} 0 0].so"
			append opt_cmd " -basicaa -silvia-${op}"
			if {${op} == "add"} {
				append opt_cmd " -silvia-add-op=${instruction} -silvia-add-simd-factor=${factor}"
			}
			if {(${op} == "mul" || ${op} == "muladd") && [dict exist ${pass} INLINE]} {
				append opt_cmd " -silvia-${op}-inline=[dict get ${pass} INLINE]"
			}
			if {${op} == "muladd" && [dict exist ${pass} MAX_CHAIN_LEN]} {
				append opt_cmd " -silvia-muladd-max-chain-len=[dict get ${pass} MAX_CHAIN_LEN]"
			}
			append opt_cmd " -dce ${dut_path}/a.o.3.bc -o ${dut_path}/a.o.3.bc"
			if {${DEBUG} == 1} {
				append opt_cmd " |& cat | tee -a ${DEBUG_FILE}"
			}

			eval exec ${opt_cmd}
		}
		exec zip -rj ${db_path}/dut.hcp ${dut_path}
		open_solution ${solution_name}
		read_checkpoint ${db_path}/dut.hcp
		::csynth_design -hw_syn
	
		foreach pass ${PASSES} {
			if {[dict exist ${pass} OP] == 0} {
				continue
			}
			set op [dict get ${pass} OP]
			if {${op} == "add"} {
				set instruction "add"
				if {[dict exist ${pass} INST]} {
					set instruction [dict get ${pass} INST]
				}
				set operator "+"
				if {${instruction} == "sub"} {
					set operator "-"
				}
				set factor 2
				if {[dict exist ${pass} FACTOR]} {
					set factor [dict get ${pass} FACTOR]
				}
				foreach lang "verilog vhdl" {
					foreach dir "syn impl" {
						if {${lang} == "verilog"} {
							set extension "v"
						} else {
							set extension "vhd"
						}
						set name "_simd_${instruction}_${factor}"
						foreach f [glob -nocomplain ${project_path}/${solution_name}/${dir}/${lang}/*${name}*.${extension}] {
							set fbasename [file tail $f]
							if {[regexp "^(.*${name}.*)\.${extension}\$" $fbasename -> module_name]} {
								file copy -force ${ROOT}/template/${op}/${op}_${factor}.${extension} $f
								exec sh -c "sed -i 's/\{\{module_name\}\}/${module_name}/g' $f"
								exec sh -c "sed -i 's/\{\{operator\}\}/${operator}/g' $f"
							}
						}
					}
				}
			}
		}
	}
}

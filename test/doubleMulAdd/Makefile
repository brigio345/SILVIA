SYN_PATH:=./proj_vitis/solution1/

prepare_cosim:
	sed -i 's/_doubleMulAdd\ doubleMulAdd_u/_doubleMulAdd_cosim\ doubleMulAdd_u/g' \
		$(SYN_PATH)/syn/verilog/doubleMulAdd.v

prepare_export:
	sed -i 's/_doubleMulAdd_cosim\ doubleMulAdd_u/_doubleMulAdd\ doubleMulAdd_u/g' \
		$(SYN_PATH)/syn/verilog/doubleMulAdd.v
	sed -i 's/\.ap_clk2x(\w*)/\.ap_clk2x(ap_clk2x)/g' \
		$(SYN_PATH)/syn/verilog/dut.v
	sed -i 's/\.ap_clkFollower(\w*)/\.ap_clkFollower(ap_clkFollower)/g' \
		$(SYN_PATH)/syn/verilog/dut.v

move_to_kria:
	scp ./proj_kria/proj_kria.runs/impl_1/design_1_wrapper.bit \
		root@kriahlslab1:/home/ubuntu/overlay0/design_1.bit
	scp ./proj_kria/proj_kria.gen/sources_1/bd/design_1/hw_handoff/design_1.hwh \
		root@kriahlslab1:/home/ubuntu/overlay0/design_1.hwh

update_ip:
	rm -r hls-ip
	mkdir hls-ip && cp proj_vitis/solution1/impl/export.zip hls-ip/export.zip \
		&& cd hls-ip && unzip export.zip

clean:
	if [ -d hls-ip ]; then rm -r hls-ip; fi
	if [ -d proj_vitis ]; then rm -r proj_vitis; fi
	if [ -f vitis_hls.log ]; then rm vitis_hls.log; fi

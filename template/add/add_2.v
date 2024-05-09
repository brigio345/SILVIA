// ==============================================================
// Generated by Vitis HLS v2023.2
// Copyright 1986-2022 Xilinx, Inc. All Rights Reserved.
// Copyright 2022-2023 Advanced Micro Devices, Inc. All Rights Reserved.
// ==============================================================

`timescale 1ns/1ps

(* use_dsp = "simd" *)
(* use_simd = "two24" *)
(* use_mult = "none" *)
module {{prefix}}_simd_{{instruction}}_2{{suffix}} (
    input   ap_clk,
    input   ap_rst,
    input  [23:0] a0_val,
    input  [23:0] b0_val,
    input  [23:0] a1_val,
    input  [23:0] b1_val,
    output reg [23:0] ap_return_0,
    output reg [23:0] ap_return_1,
    input   ap_ce = 1
);

wire   [23:0] add_ln25_fu_48_p2;
reg   [23:0] add_ln25_reg_70;
wire   [23:0] add_ln26_fu_54_p2;
reg   [23:0] add_ln26_reg_75;
reg    ap_ce_reg;
reg   [23:0] ap_return_0_int_reg;
reg   [23:0] ap_return_1_int_reg;

always @ (posedge ap_clk) begin
    ap_ce_reg <= ap_ce;
end

always @ (posedge ap_clk) begin
    add_ln25_reg_70 <= add_ln25_fu_48_p2;
    add_ln26_reg_75 <= add_ln26_fu_54_p2;
end

always @ (posedge ap_clk) begin
    if ((1'b1 == ap_ce_reg)) begin
        ap_return_0_int_reg <= add_ln25_reg_70;
        ap_return_1_int_reg <= add_ln26_reg_75;
    end
end

always @ (*) begin
    if ((1'b0 == ap_ce_reg)) begin
        ap_return_0 = ap_return_0_int_reg;
    end else if ((1'b1 == ap_ce_reg)) begin
        ap_return_0 = add_ln25_reg_70;
    end else begin
        ap_return_0 = 'bx;
    end
end

always @ (*) begin
    if ((1'b0 == ap_ce_reg)) begin
        ap_return_1 = ap_return_1_int_reg;
    end else if ((1'b1 == ap_ce_reg)) begin
        ap_return_1 = add_ln26_reg_75;
    end else begin
        ap_return_1 = 'bx;
    end
end

assign add_ln25_fu_48_p2 = (a0_val {{operator}} b0_val);

assign add_ln26_fu_54_p2 = (a1_val {{operator}} b1_val);

endmodule //{{prefix}}_simd_{{instruction}}_2{{suffix}}

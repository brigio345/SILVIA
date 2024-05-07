// This module describes SIMD Inference 
// 4 adders packed into single DSP block
`timescale 1ns/1ps

(* use_dsp = "simd" *)
(* use_simd = "four12" *)
(* use_mult = "none" *)
(* dont_touch = "true" *)
module _simd_add_4 (
    input   ap_clk,
    input   ap_rst,
    input  [11:0] a0_val,
    input  [11:0] b0_val,
    input  [11:0] a1_val,
    input  [11:0] b1_val,
    input  [11:0] a2_val,
    input  [11:0] b2_val,
    input  [11:0] a3_val,
    input  [11:0] b3_val,
    output reg [11:0] ap_return_0,
    output reg [11:0] ap_return_1,
    output reg [11:0] ap_return_2,
    output reg [11:0] ap_return_3,
    input   ap_ce = 1
);

wire   [11:0] add_ln25_fu_80_p2;
reg   [11:0] add_ln25_reg_124;
wire   [11:0] add_ln26_fu_86_p2;
reg   [11:0] add_ln26_reg_129;
wire   [11:0] add_ln27_fu_92_p2;
reg   [11:0] add_ln27_reg_134;
wire   [11:0] add_ln28_fu_98_p2;
reg   [11:0] add_ln28_reg_139;
reg    ap_ce_reg;
reg   [11:0] ap_return_0_int_reg;
reg   [11:0] ap_return_1_int_reg;
reg   [11:0] ap_return_2_int_reg;
reg   [11:0] ap_return_3_int_reg;

always @ (posedge ap_clk) begin
    ap_ce_reg <= ap_ce;
end

always @ (posedge ap_clk) begin
    add_ln25_reg_124 <= add_ln25_fu_80_p2;
    add_ln26_reg_129 <= add_ln26_fu_86_p2;
    add_ln27_reg_134 <= add_ln27_fu_92_p2;
    add_ln28_reg_139 <= add_ln28_fu_98_p2;
end

always @ (posedge ap_clk) begin
    if ((1'b1 == ap_ce_reg)) begin
        ap_return_0_int_reg <= add_ln25_reg_124;
        ap_return_1_int_reg <= add_ln26_reg_129;
        ap_return_2_int_reg <= add_ln27_reg_134;
        ap_return_3_int_reg <= add_ln28_reg_139;
    end
end

always @ (*) begin
    if ((1'b0 == ap_ce_reg)) begin
        ap_return_0 = ap_return_0_int_reg;
    end else if ((1'b1 == ap_ce_reg)) begin
        ap_return_0 = add_ln25_reg_124;
    end else begin
        ap_return_0 = 'bx;
    end
end

always @ (*) begin
    if ((1'b0 == ap_ce_reg)) begin
        ap_return_1 = ap_return_1_int_reg;
    end else if ((1'b1 == ap_ce_reg)) begin
        ap_return_1 = add_ln26_reg_129;
    end else begin
        ap_return_1 = 'bx;
    end
end

always @ (*) begin
    if ((1'b0 == ap_ce_reg)) begin
        ap_return_2 = ap_return_2_int_reg;
    end else if ((1'b1 == ap_ce_reg)) begin
        ap_return_2 = add_ln27_reg_134;
    end else begin
        ap_return_2 = 'bx;
    end
end

always @ (*) begin
    if ((1'b0 == ap_ce_reg)) begin
        ap_return_3 = ap_return_3_int_reg;
    end else if ((1'b1 == ap_ce_reg)) begin
        ap_return_3 = add_ln28_reg_139;
    end else begin
        ap_return_3 = 'bx;
    end
end

assign add_ln25_fu_80_p2 = (b0_val + a0_val);

assign add_ln26_fu_86_p2 = (b1_val + a1_val);

assign add_ln27_fu_92_p2 = (b2_val + a2_val);

assign add_ln28_fu_98_p2 = (b3_val + a3_val);

endmodule //_simd_add_4

(* use_dsp = "simd" *)
(* use_simd = "two24" *)
(* use_mult = "none" *)
(* dont_touch = "true" *)
module _simd_add_2 (
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

assign add_ln25_fu_48_p2 = (b0_val + a0_val);

assign add_ln26_fu_54_p2 = (b1_val + a1_val);

endmodule //_simd_add_2

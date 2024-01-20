`timescale 1ns/1ps

module dotprod_x7 (
  input clk, rst, ce, 
  input [7:0] a0, d0, b0, 
  input [47:0] partialIn,
  output [47:0] partialOut);

wire [28:0] _a0 = {a0[7], a0[7], a0[7], a0[7], a0, 18'b0};

wire [27:0] _d0;
assign _d0[26:8] = {19{d0[7]}}; assign _d0[7:0] = d0;

wire [17:0] _b0;
assign _b0[17:8] = {10{b0[7]}}; assign _b0[7:0] = b0;

dsp_muladd2simd _muladd2simd_0 (
  .clk(clk),
  .ce(ce),
  .rst(rst),
  .a(_a0),
  .b(_b0),
  .d(_d0),
  .partialIn(partialIn),
  .partialOut(partialOut));

endmodule

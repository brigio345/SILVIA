// This module describes SIMD Inference 
// 4 adders packed into single DSP block
`timescale 1ns/1ps

(* use_dsp = "simd" *)
(* use_simd = "four12" *)
(* use_mult = "none" *)
(* dont_touch = "true" *)
module __add4simd (input clk, rst, ce, 
                   input  [11:0] a0, a1, a2, a3, 
                   input  [11:0] b0, b1, b2, b3,
                   output [47:0] ap_return);

wire signed [11:0] dout0 = $signed(a0) + $signed(b0);
wire signed [11:0] dout1 = $signed(a1) + $signed(b1);
wire signed [11:0] dout2 = $signed(a2) + $signed(b2);
wire signed [11:0] dout3 = $signed(a3) + $signed(b3);
assign ap_return = {dout0, dout1, dout2, dout3};

endmodule // __add4simd


`timescale 1ns/1ps
module add4simd (input clk, rst, ce, 
                 input [47:0] a, 
                 input [47:0] b,
                 output [47:0] ap_return);

__add4simd add4simd_instance (
  .clk(clk),
  .rst(rst),
  .ce(ce),
  .a0(a[47:36]),
  .a1(a[35:24]),
  .a2(a[23:12]),
  .a3(a[11:0]),
  .b0(b[47:36]),
  .b1(b[35:24]),
  .b2(b[23:12]),
  .b3(b[11:0]),
  .ap_return(ap_return));
endmodule // add4simd
  

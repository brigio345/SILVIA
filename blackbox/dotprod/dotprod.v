`timescale 1ns/1ps

(* dont_touch = "true" *)
module dotprod (input clk, rst, ce, 
                   input  [7:0] a0, a1, a2, 
                   input  [7:0] b0, b1, b2,
                   output [57:0] ap_return);

wire signed [16:0] dout0 = $signed(a0) * $signed(b0);
wire signed [16:0] dout1 = $signed(a1) * $signed(b1);
wire signed [16:0] dout2 = $signed(a2) * $signed(b2);
assign ap_return = (dout0 + dout1 + dout2);

endmodule // dotprod

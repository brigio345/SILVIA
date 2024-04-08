// This module describes SIMD Inference 
// 4 adders packed into single DSP block
`timescale 1ns/1ps

(* use_dsp = "simd" *)
(* use_simd = "four12" *)
(* use_mult = "none" *)
(* dont_touch = "true" *)
module add4simd (
        ap_ready,
        a0_val,
        b0_val,
        a1_val,
        b1_val,
        a2_val,
        b2_val,
        a3_val,
        b3_val,
        ap_return_0,
        ap_return_1,
        ap_return_2,
        ap_return_3
);

output   ap_ready;
input  [11:0] a0_val;
input  [11:0] b0_val;
input  [11:0] a1_val;
input  [11:0] b1_val;
input  [11:0] a2_val;
input  [11:0] b2_val;
input  [11:0] a3_val;
input  [11:0] b3_val;
output  [11:0] ap_return_0;
output  [11:0] ap_return_1;
output  [11:0] ap_return_2;
output  [11:0] ap_return_3;

assign ap_return_0 = (b0_val + a0_val);
assign ap_return_1 = (b1_val + a1_val);
assign ap_return_2 = (b2_val + a2_val);
assign ap_return_3 = (b3_val + a3_val);

assign ap_ready = 1'b1;

endmodule //add4simd

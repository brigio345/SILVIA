// This module describes SIMD Inference 
// 4 adders packed into single DSP block
`timescale 1ns/1ps

(* use_dsp = "simd" *)
(* use_simd = "four12" *)
(* use_mult = "none" *)
(* dont_touch = "true" *)
module dsp_add_4simd_pipe_l0_internal (input clk, rst, ce, 
                              input signed [11:0] a0, a1, a2, a3, 
                              input signed [11:0] b0, b1, b2, b3,
                              output signed [47:0] ap_return);

// reg signed [11:0] aReg [3:0];
// reg signed [11:0] bReg [3:0];
// reg signed [11:0] doutReg [3:0];
// reg apPipe [1:0];

// assign ap_return = {doutReg[0], doutReg[1], doutReg[2], doutReg[3]};

wire signed [11:0] dout0 = a0 + b0;
wire signed [11:0] dout1 = a1 + b1;
wire signed [11:0] dout2 = a2 + b2;
wire signed [11:0] dout3 = a3 + b3;
assign ap_return = {dout0, dout1, dout2, dout3};

// always @ (posedge clk) begin
// integer i;
//   if (rst) begin
//     for ( i = 0; i < 4; i = i + 1) begin
//       aReg[i] <= 0;
//       bReg[i] <= 0;
//       doutReg[i] <= 0;
//     end // for
//   end else if (ce) begin
//     // aReg[0] <= a0;
//     // aReg[1] <= a1;
//     // aReg[2] <= a2;
//     // aReg[3] <= a3;
// 
//     // bReg[0] <= b0;
//     // bReg[1] <= b1;
//     // bReg[2] <= b2;
//     // bReg[3] <= b3;
// 
//     // for (i = 0; i < 4; i = i + 1) begin
//     //   doutReg[i] <= aReg[i] + bReg[i];
//     // end // for
//     
//     doutReg[0] <= a0 + b0;
//     doutReg[1] <= a1 + b1;
//     doutReg[2] <= a2 + b2;
//     doutReg[3] <= a3 + b3;
//   end // if 
// end

endmodule // dsp_add_simd


`timescale 1ns/1ps
module dsp_add_4simd_pipe_l0 (input clk, rst, ce, 
                     input [47:0] a, 
                     input [47:0] b,
                     output [47:0] ap_return);

dsp_add_4simd_pipe_l0_internal dsp_add_4simd_pipe_l0_internal_U (
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
endmodule
  

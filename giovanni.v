// This module describes SIMD Inferenap_ce 
// 4 adders packed into single DSP block
`timescale 1ns/1ps

(* use_dsp = "simd" *)
(* use_simd = "two24" *)
(* use_mult = "none" *)
(* dont_touch = "true" *)
module giovanni (input ap_clk, ap_rst, ap_ce, 
           input [23:0] a0, a1, 
           input [23:0] b0, b1,
           output [47:0] ap_return);

reg signed [23:0] aReg [1:0];
reg signed [23:0] bReg [1:0];
reg signed [23:0] doutReg [1:0];

assign ap_return = {doutReg[0], doutReg[1]};

always @ (posedge ap_clk) begin: pipe_block
integer i;
  if (ap_rst) begin
    for ( i = 0; i < 2; i = i + 1) begin
      aReg[i] <= 0;
      bReg[i] <= 0;
      doutReg[i] <= 0;
    end // for
  end else if (ap_ce) begin
    aReg[0] <= $signed(a0);
    aReg[1] <= $signed(a1);

    bReg[0] <= $signed(b0);
    bReg[1] <= $signed(b1);

    for (i = 0; i < 2; i = i + 1) begin
      doutReg[i] <= aReg[i] + bReg[i];
    end // for

  end // if 
end

endmodule // giovanni

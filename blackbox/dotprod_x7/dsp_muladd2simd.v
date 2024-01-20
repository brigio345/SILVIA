`timescale 1ns/1ps

module dsp_muladd2simd (
  input clk, rst, ce,
  input [29:0] a,
  input [26:0] d,
  input [17:0] b,
  input [47:0] partialIn,
  output [47:0] partialOut
);

reg signed [29:0] aReg;
reg signed [26:0] dReg;
reg signed [17:0] bReg;
reg signed [26:0] preAdd;
reg signed [44:0] prod;
reg signed [47:0] partialInReg[1:0];
reg signed [47:0] partialOutReg;
always @(posedge clk) begin
  if (ce) begin
    aReg <= $signed(a);
    dReg <= $signed(d);
    preAdd <= aReg + dReg;
    bReg <= $signed(b);
    partialInReg[0] <= $signed(partialIn);

    prod <= bReg * preAdd;
    partialInReg[1] <= partialInReg[0];

    partialOutReg <= partialInReg[1] + prod;
  end // ce
end // clk

assign partialOut = partialOutReg;

endmodule

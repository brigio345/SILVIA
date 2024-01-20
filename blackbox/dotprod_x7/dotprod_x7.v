`timescale 1ns/1ps

module dotprod_x7 (
  input clk, rst, ce, 
  input [7:0] a0, a1, a2, a3, a4, a5, a6,
  input [7:0] d0, d1, d2, d3, d4, d5, d6, 
  input [7:0] b0, b1, b2, b3, b4, b5, b6, 
  output [47:0] partialOut);

wire [26:0] a[6:0];
assign a[0] = {a0[7], a0, 18'b0};
assign a[1] = {a1[7], a1, 18'b0};
assign a[2] = {a2[7], a2, 18'b0};
assign a[3] = {a3[7], a3, 18'b0};
assign a[4] = {a4[7], a4, 18'b0};
assign a[5] = {a5[7], a5, 18'b0};
assign a[6] = {a6[7], a6, 18'b0};

wire [26:0] d[6:0];
assign d[0][26:8] = {19{d0[7]}}; assign d[0][7:0] = d0;
assign d[1][26:8] = {19{d1[7]}}; assign d[1][7:0] = d1;
assign d[2][26:8] = {19{d2[7]}}; assign d[2][7:0] = d2;
assign d[3][26:8] = {19{d3[7]}}; assign d[3][7:0] = d3;
assign d[4][26:8] = {19{d4[7]}}; assign d[4][7:0] = d4;
assign d[5][26:8] = {19{d5[7]}}; assign d[5][7:0] = d5;
assign d[6][26:8] = {19{d6[7]}}; assign d[6][7:0] = d6;

wire [17:0] b[6:0];
assign b[0][17:8] = {10{b0[7]}}; assign b[0][7:0] = b0;
assign b[1][17:8] = {10{b1[7]}}; assign b[1][7:0] = b1;
assign b[2][17:8] = {10{b2[7]}}; assign b[2][7:0] = b2;
assign b[3][17:8] = {10{b3[7]}}; assign b[3][7:0] = b3;
assign b[4][17:8] = {10{b4[7]}}; assign b[4][7:0] = b4;
assign b[5][17:8] = {10{b5[7]}}; assign b[5][7:0] = b5;
assign b[6][17:8] = {10{b6[7]}}; assign b[6][7:0] = b6;

reg [26:0] aDly[6:0][5:0];
reg [26:0] dDly[6:0][5:0];
reg [17:0] bDly[6:0][5:0];
always @(posedge clk) begin: delay_chain
  integer i, j;
  if (ce) begin
    for (i = 1; i < 7; i+=1) begin
      aDly[i][0] <= a[i];
      dDly[i][0] <= d[i];
      bDly[i][0] <= b[i];
      for (j = 0; j < 5; j+=1) begin
        aDly[i][j+1] <= aDly[i][j];
        dDly[i][j+1] <= dDly[i][j];
        bDly[i][j+1] <= bDly[i][j];
      end // for j
    end // for i
  end // ce
end // clk

wire [47:0] partials[6:0];

// DSP chain.
dsp_muladd2simd dsp_instance(
  .clk(clk),
  .ce(ce),
  .rst(rst),
  .a(a[0]),
  .b(b[0]),
  .d(d[0]),
  .partialIn(48'b0),
  .partialOut(partials[0]));

genvar k;
generate 
  for (k = 1; k < 7; k+=1) begin
    dsp_muladd2simd dsp_instance(
     .clk(clk),
     .ce(ce),
     .rst(rst),
     .a(aDly[k][k-1]),
     .b(bDly[k][k-1]),
     .d(dDly[k][k-1]),
     .partialIn(partials[k-1]),
     .partialOut(partials[k]));
 end // for
endgenerate

assign partialOut = partials[6];

endmodule

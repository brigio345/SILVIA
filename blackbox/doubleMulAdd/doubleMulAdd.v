`timescale 1 ns / 1 ps

(* use_dsp = "yes" *) (* dont_touch = "yes" *) module mac_DSP(
    input clk,
    input rst,
    input ce,
    input  [27-1:0] din0,
    input  [18-1:0] din1,
    input  [48-1:0] din2,
    output [48-1:0]  dout);

wire signed [27 - 1:0]     a;
wire signed [18 - 1:0]     b;
wire signed [48 - 1:0]     c;
reg  signed [45 - 1:0]     m_reg;
reg  signed [27 - 1:0]     a_reg;
reg  signed [18 - 1:0]     b_reg;
reg  signed [48 - 1:0]     c_reg;
reg  signed [48 - 1:0]     p_reg;

assign a  = $signed(din0);
assign b  = $signed(din1);
assign c  = $signed(din2);

always @(posedge clk) begin
    if (ce) begin
      // Multiply.
      a_reg  <= a;
      b_reg  <= b;
      c_reg <= c;
      m_reg <= a_reg * b_reg;

      // Accumulate.
      p_reg  <= m_reg + c_reg;
      
      // m_reg <= a * b;
      // p_reg <= m_reg + c_reg[0];
    end
end

assign dout = p_reg;

endmodule

////////////////////////////////////////////////////////////////////////////////
`define COSIM_BLK
`ifdef COSIM_BLK
`timescale 1 ns / 1 ps

module _doubleMulAdd_cosim(
  input clk,
  input rst,
  input clk2x, 
  input rst2x, 
  input ce,
  input  [27-1:0] a0, a1,
  input  [18-1:0] b0, b1,
  input  [48-1:0] c0, c1,
  output [48-1:0] dout0, dout1);

  reg   [27-1:0] a0Reg, a1Reg;
  reg   [18-1:0] b0Reg, b1Reg;
  reg   [48-1:0] c0Reg[1:0], c1Reg[1:0];
  reg cePiped;
  // DSP signals.
  wire  [48-1:0] dout0DSP, dout1DSP;
  reg  [48-1:0] dout0BakReg, dout1BakReg;

  assign dout0 = cePiped ? dout0DSP : dout0BakReg;
  assign dout1 = cePiped ? dout1DSP : dout1BakReg;

  mac_DSP mac_DSP_0(
    .clk(clk),
    .rst(rst),
    .ce(cePiped), 
    .din0(a0Reg),
    .din1(b0Reg),
    .din2(c0Reg[1]),
    .dout(dout0DSP));

  mac_DSP mac_DSP_1(
    .clk(clk),
    .rst(rst),
    .ce(cePiped), 
    .din0(a1Reg),
    .din1(b1Reg),
    .din2(c1Reg[1]),
    .dout(dout1DSP));

  always @(posedge clk) begin
    cePiped <= ce;
    if (cePiped) begin
      a0Reg <= a0;
      b0Reg <= b0;
      c0Reg[0] <= c0;
      c0Reg[1] <= c0Reg[0];
      a1Reg <= a1;
      b1Reg <= b1;
      c1Reg[0] <= c1;
      c1Reg[1] <= c1Reg[0];
      dout0BakReg <= dout0DSP;
      dout1BakReg <= dout1DSP;
    end 
  end

endmodule
`endif

////////////////////////////////////////////////////////////////////////////////
`timescale 1 ns / 1 ps

module _doubleMulAdd(
  input clk,
  input rst,
  input clk2x, 
  input rst2x, 
  input ce,
  input  [27-1:0] a0, a1,
  input  [18-1:0] b0, b1,
  input  [48-1:0] c0, c1,
  output [48-1:0] dout0, dout1);

  reg   [27-1:0] a0RegSlow, a1RegSlow, a0RegFast, a1RegFast;
  reg   [18-1:0] b0RegSlow, b1RegSlow, b0RegFast, b1RegFast;
  reg   [48-1:0] c0RegSlow, c1RegSlow, c0RegFast[1:0], c1RegFast[1:0];
  reg   [48-1:0] dout0RegFast[1:0], dout0RegSlow;
  reg   [48-1:0] dout1RegFast[1:0], dout1RegSlow;
  reg   [48-1:0] dout0RegBak, dout1RegBak;
  // Clocked signal to drive input multiplexers.
  reg muxSelRegFast;
  // reg ceFast;
  reg cePiped;

  // DSP signals.
  wire  [27-1:0] aDSP;
  wire  [18-1:0] bDSP;
  wire  [48-1:0] cDSP;
  wire  [48-1:0] doutDSP;


  // Interface registers for the slow clock domain.
  always @(posedge clk) begin
    cePiped <= ce;
    if (cePiped) begin
      a0RegSlow <= a0;
      a1RegSlow <= a1;
      b0RegSlow <= b0;
      b1RegSlow <= b1;
      c0RegSlow <= c0;
      c1RegSlow <= c1;
      dout0RegSlow <= dout0RegFast[0];
      dout1RegSlow <= dout1RegFast[1];
      dout0RegBak <= dout0RegSlow;
      dout1RegBak <= dout1RegSlow;
    end 
  end

  // Registers to enter the fast clock domain.
  always @(posedge clk2x) begin
    // muxSelRegFast <= clk;
    if (cePiped) begin
      a0RegFast <= a0RegSlow;
      b0RegFast <= b0RegSlow;
      c0RegFast[0] <= c0RegSlow;
      c0RegFast[1] <= c0RegFast[0];
      a1RegFast <= a1RegSlow;
      b1RegFast <= b1RegSlow;
      c1RegFast[0] <= c1RegSlow;
      c1RegFast[1] <= c1RegFast[0];
      dout0RegFast[0] <= doutDSP;
      dout0RegFast[1] <= dout0RegFast[0];
      dout1RegFast[0] <= doutDSP;
      dout1RegFast[1] <= dout1RegFast[0];
    end
  end

  assign aDSP = (clk ? a0RegFast : a1RegFast);
  assign bDSP = (clk ? b0RegFast : b1RegFast);
  assign cDSP = (~clk ? c0RegFast[1] : c1RegFast[1]);
  assign dout1 = cePiped ? dout0RegSlow : dout0RegBak;
  assign dout0 = cePiped ? dout1RegSlow : dout1RegBak;

  mac_DSP mac_DSP_instance (
    .clk(clk2x),
    .rst(rst2x),
    .ce(cePiped), 
    .din0(aDSP),
    .din1(bDSP),
    .din2(cDSP),
    .dout(doutDSP));

endmodule

////////////////////////////////////////////////////////////////////////////////
`timescale 1 ns / 1 ps

module doubleMulAdd(
  input ap_clk,
  input ap_rst,
  input ap_ce,
  input ap_clk2x, 
  input  [27-1:0] a0, a1,
  input  [18-1:0] b0, b1,
  input  [48-1:0] c0, c1,
  output [96-1:0] ap_return);

wire [48-1:0] dout0, dout1;
assign ap_return = {dout0, dout1};

_doubleMulAdd doubleMulAdd_u(
  .clk(ap_clk),
  .rst(ap_rst),
  .ce(ap_ce),
  .clk2x(ap_clk2x),
  .rst2x(ap_rst),
  .a0(a0),
  .b0(b0),
  .c0(c0),
  .a1(a1),
  .b1(b1),
  .c1(c1),
  .dout0(dout0),
  .dout1(dout1));

endmodule

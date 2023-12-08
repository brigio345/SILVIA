`timescale 1 ns / 1 ps

(* use_dsp = "yes" *) module mac_DSP(
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
reg  signed [48 - 1:0]     c_reg[1:0];
reg  signed [48 - 1:0]     p_reg;

assign a  = $signed(din0);
assign b  = $signed(din1);
assign c  = $signed(din2);

always @(posedge clk) begin
    if (ce) begin
      // Multiply.
      a_reg  <= a;
      b_reg  <= b;
      c_reg[0] <= c;
      m_reg <= a_reg * b_reg;

      // Accumulate.
      c_reg[1] <= c_reg[0];
      p_reg  <= m_reg + c_reg[1];
    end
end

assign dout = p_reg;

endmodule

////////////////////////////////////////////////////////////////////////////////

`timescale 1 ns / 1 ps

module _doubleMAC(
  input clk,
  input rst,
  input clk2x, 
  input rst2x, 
  input ce,
  input  [27-1:0] a0, a1,
  input  [18-1:0] b0, b1,
  input  [48-1:0] c0, c1,
  output [48-1:0] dout0, dout1);

  
  wire  [27-1:0] _a0, _a1;
  wire  [18-1:0] _b0, _b1;
  wire  [48-1:0] _c0, _c1;

  reg [27-1:0] a0Reg, a1Reg;
  reg [18-1:0] b0Reg, b1Reg;
  reg [48-1:0] c0Reg, c1Reg;
  reg [48-1:0] dout0Reg, dout1Reg;

  wire [27-1:0] aDSP;
  wire [18-1:0] bDSP;
  wire [48-1:0] cDSP;
  wire [48-1:0] doutDSP;

  always @(posedge clk) begin
    if (ce) begin
      a0Reg <= a0;
      b0Reg <= b0;
      c0Reg <= c0;

      a1Reg <= a1;
      b1Reg <= b1;
      c1Reg <= c1;
    end 
  end

  assign aDSP = clk ? a0 : a1; 
  assign bDSP = clk ? b0 : b1; 
  assign cDSP = clk ? c0 : c1; 

  mac_DSP mac_DSP_instance (
    .clk(clk2x),
    .rst(rst2x),
    .ce(ce), 
    .din0(aDSP),
    .din1(bDSP),
    .din2(cDSP),
    .dout(doutDSP));

  always @(posedge clk2x) begin: dout0_reg
    if (ce && ~clk)
      dout0Reg <= doutDSP;
    if (ce && clk) 
      dout1Reg <= doutDSP;
  end
      
  assign dout0 = dout0Reg;
  assign dout1 = dout1Reg;

endmodule

////////////////////////////////////////////////////////////////////////////////

`timescale 1 ns / 1 ps

module doubleMAC(
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

_doubleMAC doubleMAC_u(
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

`timescale 1ns/1ps

`define N 8
`define M `N*2
`define P `M+1

module tb;
  reg clk = 0;
  reg clk_x2 = 0;
  reg rst=1, ce=1;
  reg [`N-1:0] inputs[5:0];
  wire  [27-1:0] a0, a1;
  wire  [18-1:0] b0, b1;
  wire  [48-1:0] c0, c1;
  wire signed [`P-1:0] dout0, dout1;


  initial begin
    #20 rst = 0;
  end

  assign a0=$signed(inputs[0]);
  assign b0=$signed(inputs[1]);
  assign c0=$signed(inputs[2]);

  assign a1=$signed(inputs[3]);
  assign b1=$signed(inputs[4]);
  assign c1=$signed(inputs[5]);

  // Instantiate the design under test (DUT)
  // Replace my_design with your actual module name
  doubleMAC dut(
    .clk(clk), 
    .clk_x2(clk_x2), 
    .rst(rst), 
    .rst_x2(rst), 
    .ce(ce), 
    .a0(a0), 
    .b0(b0), 
    .c0(c0), 
    .a1(a1), 
    .b1(b1), 
    .c1(c1), 
    .dout0(dout0), 
    .dout1(dout1));

  // Generate the two clocks
  always #1 clk_x2 = ~clk_x2;
  always @ (posedge clk_x2) begin clk <= ~clk; end

  // Generate the inputs at each clk1 cycle and reset
  always @(posedge clk) begin: input_gen_fsm
    integer i;
    if (rst) begin
      for(i=0; i<6; i=i+1) begin
        inputs[i] = 0;
      end
    end else if (ce) begin
      for(i=0; i<6; i=i+1) begin
        inputs[i] = $signed($urandom % 128);
      end
    end
  end

  reg signed [`N-1:0] _a0[2:0], _b0[2:0], _c0[2:0], _a1[2:0], _b1[2:0], _c1[2:0];
  reg signed [`M-1:0] m0, m1;
  reg signed [`P-1:0] p0, p1;

  always @ (posedge clk) begin
    if (ce) begin
      _a0[0] <= a0;
      _b0[0] <= b0;
      _c0[0] <= c0;

      _a0[1] <= _a0[0];
      _b0[1] <= _b0[0];
      _c0[1] <= _c0[0];
      m0 <= a0 * b0; // _a0[0] * _b0[0]; 

      _a0[2] <= _a0[1];
      _b0[2] <= _b0[1];
      _c0[2] <= _c0[1];
      p0 <= m0 + _c0[0]; // _c0[1];

      _a1[0] <= a1;
      _b1[0] <= b1;
      _c1[0] <= c1;

      _a1[1] <= _a1[0];
      _b1[1] <= _b1[0];
      _c1[1] <= _c1[0];
      m1 <= a1 * b1; // _a1[0] * _b1[0]; 

      _a1[2] <= _a1[1];
      _b1[2] <= _b1[1];
      _c1[2] <= _c1[1];
      p1 <= m1 + _c1[0]; // _c1[1];
    end 
  end

  // Check the outputs at each clk1 or clk2 cycle
  always @(posedge clk) begin
    if (!rst && ce) begin
      $display("DUT: (%d, %d) | GOLD: (%d, %d).", dout0, dout1, p0, p1);
    end
  end
endmodule


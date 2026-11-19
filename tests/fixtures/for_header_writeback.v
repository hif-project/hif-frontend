// A combinational for loop over a module-level integer - a population count,
// which is the plainest possible use of the construct.
//
// *** THIS DESIGN CURRENTLY FAILS, DELIBERATELY. ***
//
// hif2verilog shadows the loop index into `i_sig_var_0` and appends the
// write-back to `i` onto the init and step clauses with a comma:
//
//   for (i_sig_var_0 = 0, i = i_sig_var_0; i_sig_var_0 < 8;
//        i_sig_var_0 = i_sig_var_0 + 1, i = i_sig_var_0)
//
// Verilog has no comma operator, so neither verilog2hif nor iverilog will take
// it - hif-backend#80. No generate, no disable, no unusual construct: this is
// what an ordinary iterative combinational function regenerates as today.
module for_loop_integer(input [7:0] d, output reg [3:0] cnt);
  integer i;

  always @(*) begin
    cnt = 4'd0;
    for (i = 0; i < 8; i = i + 1) begin
      if (d[i]) cnt = cnt + 4'd1;
    end
  end
endmodule

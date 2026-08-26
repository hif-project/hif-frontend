// Regression fixture for hif-frontend#30, strided form.
//
// A `for generate` whose index advances by more than one, driving a net that is
// read from outside the loop. That is the shape #30 fixed for unit-step loops:
// generateConeFunctions places the cone for `w` at module scope and fills it
// with copies of the drivers, which live inside the loop and refer to names
// declared there, so the cone does not resolve:
//
//   [HIF] [getDeclaration] - ERROR: Declaration not found
//     -- in SubProgram: hif_cone_w_0
//
// #30 could not expand this loop while hif-core#24 was open, because expansion
// substituted the iteration *ordinal* for the genvar: this loop drives bits 0,
// 2 and 4, and would have elaborated as bits 0, 1 and 2 - a different design,
// at exit 0, with no diagnostic. It was therefore left unexpanded and kept the
// abort above, a loud failure in preference to a silently different one.
//
// hif-core#24 is fixed (hif-core 5603a2a), so the loop is expanded and the
// substituted values are the ones the genvar actually takes.
//
// The reader is what makes exit status mean something here: an earlier version
// of this fixture had none and translated at exit 0 either way, so it could
// only assert structure. This one aborts before the change and translates
// after it.
//
// `always @(posedge clk)` rather than a continuous read, matching
// genvar_cone_reader.v: a process is the shape hif-frontend#30 reports.
//
// Bits 1, 3 and 5 of `w` are deliberately left undriven - that is what a
// strided loop means, and it is the property the ordinal bug destroyed.
//
// iverilog -g2005 accepts this file.
module genvar_nonunit_step (input clk, input [5:0] a, output reg [5:0] q);
  wire [5:0] w;
  genvar g;
  generate
    for (g = 0; g < 6; g = g + 2) begin : blk
      assign w[g] = ~a[g];
    end
  endgenerate
  always @(posedge clk) q <= w;
endmodule

// Regression fixture for hif-frontend#30: verilog2hif aborted on a genvar-driven
// net that is read from outside the loop.
//
//   [HIF] [getDeclaration] - ERROR: Declaration not found
//     -- in SubProgram: hif_cone_w_0
//
// The cone named is the one for `w` itself, not for a split signal, and that is
// what separates this from #23. generateConeFunctions places a cone beside the
// declaration it drives - `w`, a module-scope wire - and fills it with copies of
// the driver assignments. Those drivers live inside the loop, so the copies
// carry references to names declared there: the genvar, and (since #23 put it
// there) the split signal. Neither resolves at module scope.
//
// The cone cannot follow them into the loop: its caller is the module-scope
// process below, so it has to stay where the caller can see it. That is why #23's
// fix does not reach this shape and why the loop is expanded instead.
//
// `always @(posedge clk)` rather than a continuous read: a process is the shape
// the issue reports. A continuous-assign reader fails identically - the issue
// text claiming a process is required was wrong - and that form is a control in
// genvar_continuous_assign_controls.v.
//
// The target varies with the genvar, so expansion produces `w[0]` and `w[1]`.
// A loop body writing a fixed bit would produce two structurally identical
// targets and trip hif-frontend#39 instead, which is a different defect.
//
// The loop steps by one. A non-unit step is deliberately not expanded, because
// expansion substitutes the iteration ordinal rather than the genvar's value
// (hif-core#24); genvar_generate_controls covers that.
//
// iverilog -g2005 accepts this file.
module genvar_cone_reader (input clk, input [1:0] a, output reg [1:0] q);
  wire [1:0] w;
  genvar g;
  generate
    for (g = 0; g < 2; g = g + 1) begin : blk
      assign w[g] = ~a[g];
    end
  endgenerate
  always @(posedge clk) q <= w;
endmodule

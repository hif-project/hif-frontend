// Regression fixture for hif-frontend#23: verilog2hif aborted on a continuous
// assignment inside a `for generate` whose right-hand side or target index
// reads the genvar.
//
// splitLogicConesLoops replaces the bit-select target with a fresh signal, and
// generateConeFunctions later copies the assignment into a procedure declared
// beside that signal. The signal was declared beside its *target* - module
// scope - so the copy left the genvar's scope and standardization aborted on
// the unresolvable reference, at "STD 01: Simplifying source tree".
//
// One module, one loop, one assignment: the structural check in the test needs
// exactly one ForGenerate to point at. The forms that already worked - a
// procedural body, a continuous assignment that does not read the genvar, an
// if generate, a module instance - are controls and live in
// genvar_continuous_assign_controls.v, so that a change breaking one of them
// fails a differently-named test.
//
// A single-iteration loop fails identically, so this is not about having
// several drivers; two iterations are used here because that is also the case
// where one module-scope signal would be shared by both.
//
// iverilog -g2005 accepts this file.
module genvar_continuous_assign (input [1:0] a, output [1:0] y);
  genvar g;
  generate
    for (g = 0; g < 2; g = g + 1) begin : blk
      assign y[g] = ~a[g];
    end
  endgenerate
endmodule

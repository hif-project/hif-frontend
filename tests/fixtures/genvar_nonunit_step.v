// Control for hif-frontend#30: a `for generate` whose index does NOT advance by
// one must be left unexpanded.
//
// Expansion substitutes the iteration *ordinal* for the genvar rather than the
// value the genvar takes, so this loop - which drives bits 0, 2 and 4 - would
// elaborate as bits 0, 1 and 2. Exit 0, no diagnostic, a different design.
// That is hif-core#24, and it is why #30's expansion is restricted to unit-step
// loops.
//
// This fixture has no reader outside the loop, so it translates at exit 0 both
// before and after #30. Exit status therefore proves nothing here and the test
// is structural: it asserts the loop is still a loop. If the unit-step guard is
// ever removed while hif-core#24 is open, this fails instead of quietly
// producing the wrong design.
//
// iverilog -g2005 accepts this file.
module genvar_nonunit_step (input [5:0] a, output wire [5:0] y);
  genvar g;
  generate
    for (g = 0; g < 6; g = g + 2) begin : blk
      assign y[g] = ~a[g];
    end
  endgenerate
endmodule

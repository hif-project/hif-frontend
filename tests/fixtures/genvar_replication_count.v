// A genvar is constant within its generate loop (IEEE Std 1364-2005, 12.1.3.2),
// so `{g{1'b1}}` is a legal replication and hif-frontend#20's check must not
// report it. It is modelled as a hif::Variable carrying PROPERTY_GENVAR at the
// point the check runs, which is why the check needs an explicit exemption for
// it rather than falling through the signal/port/variable branch.
//
// This fixture is NOT expected to translate: a continuous assignment reading a
// genvar inside a `for generate` aborts for an unrelated reason - the cone
// procedure it is lifted into is declared at module scope, where the genvar is
// invisible - which is hif-frontend#23. The test therefore asserts only that
// the failure is not the non-constant-count diagnostic, and will start
// asserting exit 0 when #23 is fixed.
module genvar_replication_count (output [7:0] o);
  wire [7:0] w [0:1];
  genvar g;
  generate
    for (g = 1; g < 3; g = g + 1) begin : blk
      assign w[g-1] = {g{1'b1}};
    end
  endgenerate
  assign o = w[0] | w[1];
endmodule

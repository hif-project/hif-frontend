// Companion to if_generate_instance.v, for the half that hangs rather than
// aborts (hif-frontend#32).
//
// Here the instance is in the branch that LOSES. Once hif-core folds the
// generate condition, the losing branch is removed and
// `if_generate_dead_branch_leaf` is left defined but instantiated by nothing.
//
// partialFlattening's loop then never terminates. collectOnAssigns still
// collects the leaf's view, because it still drives an output port with a
// continuous assignment, so `needsFlattening` stays true; but there is no
// instance left to flatten, so the round changes nothing and the next one
// collects exactly the same set. Measured before the fix: identical
// `views={leaf, top}, viewRefs={}` from the second iteration onwards, flat
// memory, about 430 iterations a second, indefinitely.
//
// This shape was unreachable before hif-core's fold, because the abort in
// if_generate_instance.v came first. That is why the two fixtures are separate:
// one covers "it elaborates", the other covers "elaborating it terminates", and
// a regression in either should say which.
//
// The test carries a TIMEOUT, because the failure mode is a hang and a
// regression must fail rather than wedge the suite.
//
// iverilog -g2005 accepts this file.
module if_generate_dead_branch_leaf(input [3:0] a, input [3:0] b, output [3:0] y);
  assign y = a & b;
endmodule

module if_generate_dead_branch_instance (input [3:0] a, input [3:0] b, output [3:0] y);
  generate
    if (1) begin : chosen
      assign y = a & b;
    end else begin : discarded
      if_generate_dead_branch_leaf u (.a(a), .b(b), .y(y));
    end
  endgenerate
endmodule

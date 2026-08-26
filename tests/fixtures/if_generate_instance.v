// Regression fixture for hif-frontend#32: verilog2hif aborted on an
// `if generate` with an else branch.
//
//   [HIF] [Simplify] - ASSERT: Cannot resolve if generate condition
//   - Raised by hif-core/src/manipulation/simplify.cpp:4399
//
// The condition is not what could not be resolved, despite the message.
// Verilog lowers `if (<cond>)` to `or_reduce(<cond>)` and the else branch is
// built as the negation of that, and hif-core's constant folder had a `// TODO`
// where a reduction over a constant should be folded - so the else condition
// stayed an Expression where _simplifyIfGenerate needs a ConstValue. Fixed in
// hif-core; this fixture is what keeps it fixed from this side.
//
// The instance is what makes the failure reachable rather than what causes it:
// it is only because the module contains one that partialFlattening runs
// flattenDesign, which asks for generate expansion in the first place. The same
// `if generate` with no instance anywhere in the module translated at exit 0
// before the fix - not because it was handled, but because nothing ever tried
// to elaborate it. genvar_generate_controls holds that form.
//
// USE_LEAF defaults to 1, so the leaf branch is the one that survives and gets
// flattened in: the check below requires the `&` from the leaf and refuses the
// `|` from the else branch, so selecting the wrong branch fails rather than
// passing on exit status.
//
// iverilog -g2005 accepts this file.
module if_generate_instance_leaf(input [3:0] a, input [3:0] b, output [3:0] y);
  assign y = a & b;
endmodule

module if_generate_instance #(parameter USE_LEAF = 1) (input [3:0] a, input [3:0] b, output [3:0] y);
  generate
    if (USE_LEAF) begin : with_leaf
      if_generate_instance_leaf u (.a(a), .b(b), .y(y));
    end else begin : without_leaf
      assign y = a | b;
    end
  endgenerate
endmodule

// Fixture for hif-frontend#14: verilog2hif aborted with "Declaration not
// found" on a user-defined function.
//
// The trigger is narrower than the issue's title suggests, and the shape here
// is chosen to hit it exactly: the call has to be in a *continuous assignment
// whose target is an output port*. A call inside an always block, or a
// continuous assignment to an internal wire, always worked.
//
// Cause: an assignment whose right-hand side looked constant was folded into
// the target's declaration. For an output port that declaration is on the
// Entity, which cannot see anything declared in the Contents - so the call to
// `bump`, whose Function is a Contents declaration, was stranded with nothing
// to resolve to, and the whole-system reference pass aborted.
module user_function_continuous_assign(output [3:0] o, output [3:0] p);

  function [3:0] bump;
    input [3:0] v;
    begin
      bump = v + 4'd1;
    end
  endfunction

  // The reported shape.
  assign o = bump(4'd3);

  // A second call, to show the fix is not specific to one call site and that
  // the function may be called more than once.
  assign p = bump(4'd7);

endmodule

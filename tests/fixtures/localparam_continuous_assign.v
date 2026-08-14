// Second fixture for hif-frontend#14, and the evidence that the defect was
// never about functions.
//
// A `localparam` is a Contents declaration too, so a continuous assignment of
// one to an output port was folded onto the Entity in exactly the same way and
// aborted with exactly the same "Declaration not found". No function involved.
//
// The contrast cases are here on purpose. A module parameter is a View
// template parameter and a system function lives in a LibraryDef under the
// System, so both were always reachable from the Entity and both kept folding
// as before - if the fix had been written as "never fold", these would have
// changed behaviour and this fixture would not have noticed.
module localparam_continuous_assign #(parameter [3:0] P = 4'd9) (
    output [3:0]  o,
    output [3:0]  q,
    output [31:0] r,
    output [3:0]  s
);

  localparam [3:0] K = 4'd5;

  // Was broken: K is declared in the Contents.
  assign o = K;

  // Always worked, and must keep working: a module parameter.
  assign q = P;

  // Always worked, and must keep working: a system function.
  assign r = $clog2(32'd17);

  // Always worked, and must keep working: a plain literal.
  assign s = 4'd2;

endmodule

// SystemVerilog `logic` carrying `signed`, in a body declaration.
//
// Regression fixture for hif-frontend#34: this was a bare `syntax error`, exit
// 134, no artifact - while the same declaration written with `reg` translated
// fine and `logic [3:0]` without `signed` translated fine. So it was neither
// `logic` nor `signed` but the two together.
//
// `logic` is not a lexer keyword: a body declaration written with it has no
// declaration rule of its own - the net_declaration alternative that would take
// it is commented out, pointing at module_instantiation - so it arrives as a
// module instantiation whose module identifier is the type name. `signed` had
// no place in that syntax, so the parse failed before any refinement could run.
//
// Both widths are here because they take different paths through the fix. The
// vector carries the signedness on the Array that FixDescription_1 folds into a
// Bitvector; the scalar has no Array at all, and must come out exactly as
// `reg signed s;` does - which is an unsigned Bit, because a one-bit `signed`
// is dropped on the `reg` path too. Parity with `reg` is the requirement, not
// signedness in the abstract, and pinning the scalar is what stops a future
// change from "improving" one spelling out of step with the other.
//
// iverilog -g2012 accepts this file.
module sv_logic_signed (clk, a, b, y, z);
input clk;
input [3:0] a;
input b;
output [3:0] y;
output z;

logic signed [3:0] wide;
logic signed       narrow;

always @(posedge clk) begin
    wide   <= a;
    narrow <= b;
end

assign y = wide;
assign z = narrow;
endmodule

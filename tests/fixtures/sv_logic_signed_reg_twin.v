// Twin of sv_logic_signed.v with `reg` in place of `logic`. In SystemVerilog
// the two spell the same four-state variable, so the HIF produced for this file
// and for its `logic` counterpart must agree exactly - including the signedness
// of the vector, and including the scalar, where `signed` is dropped on both
// spellings because HIF's Bit has no sign. The test diffs the two translations
// to enforce that (hif-frontend#34).
module sv_logic_signed (clk, a, b, y, z);
input clk;
input [3:0] a;
input b;
output [3:0] y;
output z;

reg signed [3:0] wide;
reg signed       narrow;

always @(posedge clk) begin
    wide   <= a;
    narrow <= b;
end

assign y = wide;
assign z = narrow;
endmodule

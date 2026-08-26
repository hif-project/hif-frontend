// SystemVerilog `logic` used as an ordinary RTL type, scalar and vector.
//
// Reduced from the counter DUT of the systems-verification course laboratory
// (countersvt/rtl/counter.sv), which declares `logic [3:0] value;`. The
// `always_ff` of the original is written `always` here so that the fixture
// isolates the type: `always_ff` is a separate, unrelated gap.
module sv_logic_type (clk, start, in, dec, stop);
input clk, start, dec;
input [3:0] in;
output stop;
logic [3:0] value;
logic busy;

always @(posedge clk) begin
    if (start) begin
        value <= in;
        busy <= 1'b1;
    end else if (dec && !stop) begin
        value <= value - 1'b1;
    end
end

assign stop = (value == 4'b0);
endmodule

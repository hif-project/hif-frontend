// Twin of sv_logic_type.v with `reg` in place of `logic`. In SystemVerilog the
// two spell the same four-state variable, so the HIF produced for this file and
// for its `logic` counterpart must agree - same type, and same uninitialised
// value. The test diffs the two translations to enforce that.
module sv_logic_type (clk, start, in, dec, stop);
input clk, start, dec;
input [3:0] in;
output stop;
reg [3:0] value;
reg busy;

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

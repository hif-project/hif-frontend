// Verilog-2001 twin of sv_logic_ansi_port.v: identical but for the keyword.
//
// `logic` and `reg` spell the same four-state variable, and an `input logic`
// port carries no more than a plain `input` does, so the two translations must
// agree exactly - not merely both exit 0. A fix that made the header parse but
// gave the ports a different type, signedness or default value would pass an
// exit-code check and fail this one.
module sv_logic_ansi_port (
    input  clk,
    input  [3:0] d,
    input  signed [3:0] s,
    output reg [3:0] q
);
    always @(posedge clk) q <= d + s;
endmodule

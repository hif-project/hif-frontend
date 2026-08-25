// The `inout` direction of the ANSI `logic` header, kept separate from
// sv_logic_ansi_port.v because its twin has to be a different spelling.
//
// An `inout logic` port is NOT equivalent to a plain `inout`: `logic` is a
// variable, and a variable's uninitialised value is 'X' where a net's is 'Z'.
// That is the distinction #21 established, so the honest reference here is the
// already-supported *body* spelling of the same thing, not the net form.
module sv_logic_ansi_inout (
    input logic en,
    inout logic [3:0] io
);
    assign io = en ? 4'b0101 : 4'bzzzz;
endmodule

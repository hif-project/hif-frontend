// SystemVerilog `logic` in an ANSI-style port header.
//
// #21 made `logic` work as a body declaration. In an ANSI port header the
// lexer still hands it over as an identifier, and the header reached the
// Verilog-AMS `discipline_and_modifiers` rule, whose bare-IDENTIFIER
// alternative refused outright:
//
//   -- ERROR: discipline_and_modifiers: IDENTIFIER is not supported
//
// Exit by signal, no output file, for what is the ordinary way a
// SystemVerilog design writes its ports.
//
// `signed` is here because it goes through the same header path. Note the
// *body* spelling of that one - `logic signed [3:0] s;` - is still rejected;
// that is a separate pre-existing gap, filed as #34, and it is why this
// fixture's twin is written in Verilog-2001 rather than with a body `logic`.
module sv_logic_ansi_port (
    input  logic clk,
    input  logic [3:0] d,
    input  logic signed [3:0] s,
    output logic [3:0] q
);
    always @(posedge clk) q <= d + s;
endmodule

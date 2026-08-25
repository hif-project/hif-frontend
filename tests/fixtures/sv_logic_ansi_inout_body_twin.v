// Body-declared twin of sv_logic_ansi_inout.v: the non-ANSI spelling of the
// same module, which #21 already supports.
//
// Requiring these two to produce the same HIF is the exact claim the ANSI fix
// makes - that a port header naming `logic` means what a body declaration
// naming `logic` already means - and it pins the 'X' default rather than
// leaving it to be re-derived by the next reader.
module sv_logic_ansi_inout (en, io);
    input en;
    inout [3:0] io;
    logic en;
    logic [3:0] io;
    assign io = en ? 4'b0101 : 4'bzzzz;
endmodule

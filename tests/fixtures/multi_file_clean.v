// A trivially valid module, used alongside escaped_identifier.v to check
// whether verilog2hif's per-file parser state survives correctly across
// multiple files given in a single invocation, in both orders.
module multi_file_clean (a, b, y);
    input a, b;
    output y;
    assign y = a | b;
endmodule

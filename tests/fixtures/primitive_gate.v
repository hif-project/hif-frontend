// Regression fixture: Verilog's built-in gate primitives (and/nand/or/nor/
// xor/xnor/not/buf) are not supported by the current grammar at all -- both
// named and unnamed instance forms are rejected. This blocks any
// structural/technology-mapped netlist, including the classic ISCAS85/89
// benchmark families (e.g. c17.v), which are built entirely from primitives
// like this.
module top (a, b, c, y);
    input a, b, c;
    output y;
    wire n1;

    nand n1_gate (n1, a, b);
    and  y_gate  (y, n1, c);
endmodule

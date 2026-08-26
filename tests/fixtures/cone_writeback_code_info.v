// Regression fixture for hif-muffin#24: the write-back assignment
// splitLogicConesLoops synthesizes for a part-select continuous assign carried
// no source attribution, so two fault locations on one signal were reported
// with byte-identical records.
//
// splitLogicConesLoops rewrites `assign y[7:4] = a[3:0]` into three things: a
// fresh split signal `y_partial0_0`, the original assignment retargeted to it -
// which keeps its own code info and was always attributed correctly - and a
// *new* write-back assignment `y[7:4] = y_partial0_0`. Only the write-back was
// created without code info, so every consumer that reads a statement's
// position off the Assign saw file "" and line 0 for it. muffin's fault report
// is where that became load-bearing; hif-core's own diagnostics printed
// "No source file info available" for the same node.
//
// Two assignments to *different part-selects of one signal*, because that is
// what makes the omission observable rather than merely untidy: with one
// location per signal an empty position is still unique, and with two it is
// not. The halves are swapped so neither assignment can be folded into the
// other or reordered without changing the design, and the target is an output
// port because that is the reported shape.
//
// The procedural form of this design was always attributed correctly and is not
// duplicated here - it is a control in hif-muffin's own suite, where the two
// forms' fault reports can be compared directly, which is the comparison worth
// making.
//
// No round-trip leg, and none is needed: this defect lives entirely in the HIF,
// and the regenerated Verilog is byte-identical before and after the fix (no
// backend reads CodeInfo). Worth knowing while reading this fixture, though:
// `hif2vhdl` aborts on it, before and after, because it cannot print any
// part-select assignment target at all. That is hif-backend#103 and is
// independent - the procedural form aborts identically.
//
// iverilog -g2005 accepts this file.
module cone_writeback_code_info (input [7:0] a, output [7:0] y);
  assign y[7:4] = a[3:0];
  assign y[3:0] = a[7:4];
endmodule

// The reproducer of hif-frontend#20. `$random` is a system *function* reporting
// simulation state, so its value is not known statically and it cannot be the
// repeat count of a replication (IEEE Std 1364-2005, 5.1.14). iverilog rejects
// this file with "Concatenation repeat expression is not constant".
module nonconst_replication_system_function (output [7:0] o);
  assign o = {$random{1'b1}};
endmodule

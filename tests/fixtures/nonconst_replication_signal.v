// The same rule, reached by the other route: the count reads a port. Filed as
// part of hif-frontend#20 alongside the $random case because both were accepted
// silently, but they take different paths through the check - this one is caught
// on the declaration kind, not on the callee name. iverilog rejects it with
// "A reference to a wire or reg ('s') is not allowed in a constant expression".
module nonconst_replication_signal (input [3:0] s, output [7:0] o);
  assign o = {s{1'b1}};
endmodule

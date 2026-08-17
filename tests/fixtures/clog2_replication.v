// hif-frontend#15: a replication whose repeat count is $clog2 of a parameter.
// The idiomatic way to build a parameterised all-ones mask. Both halves work
// alone - see system_function_call.v for $clog2 and the baseline fixture for a
// plain parameter replication - it is the combination that used to abort.
module clog2_replication #(parameter DEPTH = 32) (output [7:0] o);
  assign o = {$clog2(DEPTH){1'b1}};
endmodule

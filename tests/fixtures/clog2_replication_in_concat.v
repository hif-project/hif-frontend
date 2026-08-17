// hif-frontend#15: the same replication, this time as one element of a larger
// concatenation, so the construct is not the whole right-hand side.
module clog2_replication_in_concat #(parameter DEPTH = 32) (output [15:0] o);
  assign o = {4'ha, {$clog2(DEPTH){1'b1}}, 3'b010, 4'b0000, 1'b1};
endmodule

// hif-frontend#15 guard: $clog2 as the actual of a module parameter. This was
// suspected to be a second exposure of the same defect but never reproduced a
// failure - it translated before the fix as well. Kept so that the suspicion
// stays measured rather than re-argued.
module clog2_module_actual_leaf #(parameter W = 4) (output [W-1:0] o);
  assign o = {W{1'b1}};
endmodule

module clog2_module_actual #(parameter DEPTH = 64) (output [7:0] o);
  clog2_module_actual_leaf #(.W($clog2(DEPTH))) u (.o(o[5:0]));
  assign o[7:6] = 2'b00;
endmodule

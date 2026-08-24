// Controls for hif-frontend#23. Every form of `generate` that already
// translated before the fix, kept together so that breaking one of them fails
// here rather than silently widening the fix's blast radius.
//
// The fix moves a split signal into the enclosing ForGenerate. These modules
// pin the two ways that must NOT happen:
//
//   ctl_if_generate     an IfGenerate is not a ForGenerate - no index, not
//                       replicated - so its split signal stays at module
//                       scope, where hif2verilog renders it correctly. Moving
//                       it would land the declaration somewhere that backend
//                       gets wrong (hif-backend#78).
//
//   ctl_no_genvar       a continuous assignment inside a for generate that
//                       does not read the genvar. It has no bit-select target,
//                       so it is never split at all.
//
// The remaining two are the forms the issue's acceptance matrix lists as
// working, kept so the matrix stays covered:
//
//   ctl_procedural      a procedural body reading the genvar
//   ctl_instance        a module instance bound to genvar-indexed nets
//
// iverilog -g2005 accepts this file.
module ctl_if_generate (input [1:0] a, output [1:0] y);
  generate
    if (1) begin : blk
      assign y[0] = ~a[0];
    end
  endgenerate
  assign y[1] = a[1];
endmodule

module ctl_no_genvar (input [1:0] a, output [1:0] y);
  genvar g;
  generate
    for (g = 0; g < 2; g = g + 1) begin : blk
      assign y = ~a;
    end
  endgenerate
endmodule

module ctl_procedural (input clk, input [1:0] d, output reg [1:0] q);
  genvar g;
  generate
    for (g = 0; g < 2; g = g + 1) begin : blk
      always @(posedge clk) q[g] <= d[g];
    end
  endgenerate
endmodule

module ctl_inv (input i, output o);
  assign o = ~i;
endmodule

module ctl_instance (input [1:0] a, output [1:0] y);
  genvar g;
  generate
    for (g = 0; g < 2; g = g + 1) begin : blk
      ctl_inv u (.i(a[g]), .o(y[g]));
    end
  endgenerate
endmodule

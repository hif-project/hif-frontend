// hif-frontend#15 guard: replication and concatenation shapes that already
// translated before the fix and must keep translating - a constant count, a
// parameter count, a nested replication, and a plain concatenation.
module replication_baseline #(parameter N = 4) (output [31:0] o);
  wire [7:0] a = {4{2'b10}};
  wire [7:0] b = {N{1'b1}};
  wire [7:0] c = {2{{2{2'b01}}}};
  wire [7:0] d = {4'h5, 4'ha};
  assign o = {a, b, c, d};
endmodule

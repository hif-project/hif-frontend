// Replication counts that are legal constant expressions and must keep
// translating. This is the over-rejection guard for hif-frontend#20: the check
// added there walks the symbols of the count, and each of these is a symbol it
// has to let through.
//
//   N       a module parameter    -> ValueTP
//   N + 1   an expression over it -> still constant
//   L       a localparam          -> Const in the Contents
//   dbl(N)  a user function call  -> IEEE Std 1364-2005, 13.7 constant function
//
// $clog2 is the fifth legal shape and is deliberately absent: it has its own
// regression, clog2_replication, which pins that the count stays symbolic.
module const_replication_counts #(parameter N = 2) (output [63:0] o);
  localparam L = 3;

  function integer dbl;
    input integer x;
    begin dbl = x * 2; end
  endfunction

  wire [63:0] a = {N{1'b1}};
  wire [63:0] b = {(N+1){1'b1}};
  wire [63:0] c = {L{1'b1}};
  wire [63:0] d = {dbl(N){1'b1}};

  assign o = a | b | c | d;
endmodule

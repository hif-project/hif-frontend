module multiple_bitselect_assigns(input [3:0] in);
   wire [3:0] b;
   assign b[0] = in[0];
   assign b[1] = in[1];
endmodule

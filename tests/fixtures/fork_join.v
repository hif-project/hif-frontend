// A parallel block, which Verilog-2001 accepts and HIF cannot represent.
module fork_join (output reg y);
  initial begin
    fork
      #1 y = 1'b0;
      #2 y = 1'b1;
    join
  end
endmodule

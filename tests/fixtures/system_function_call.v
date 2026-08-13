module system_function_call #(parameter N = 16)
   (input [$clog2(N)-1:0] sel,
    output y
    );

   assign y = sel[0];

endmodule

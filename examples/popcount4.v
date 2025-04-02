`timescale 1ns / 1ps

module popcount4 (
    input  wire [3:0] data,
    output reg  [2:0] count
);
    // Function to count the number of 1s in the input.
    function [2:0] popcount;
        reg [31:0] memory;
        input [3:0] val;
        integer i;
        begin
            popcount = 0;
            for (i = 0; i < 4; i = i + 1) begin
                popcount = popcount + val[i];
            end
        end
    endfunction

    always @(*) begin
        count = popcount(data);
    end
endmodule

// ============================================================================
// Testbench
// ============================================================================

//`define ENABLE_TB

`ifdef ENABLE_TB

module popcount4_tb;
    reg  [3:0] data;
    wire [2:0] count;

    // Instantiate the module
    popcount4 dut (
        .data(data),
        .count(count)
    );

    initial begin
        $display("Time | data | count");
        $display("-----+------+------");

        data = 4'b0000; #10;
        $display("%4dns | %b |   %0d", $time, data, count);

        data = 4'b0001; #10;
        $display("%4dns | %b |   %0d", $time, data, count);

        data = 4'b1010; #10;
        $display("%4dns | %b |   %0d", $time, data, count);

        data = 4'b1111; #10;
        $display("%4dns | %b |   %0d", $time, data, count);

        data = 4'b0101; #10;
        $display("%4dns | %b |   %0d", $time, data, count);

        $finish;
    end
endmodule

`endif

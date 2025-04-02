`timescale 1ns / 1ps

// ============================================================================
// 4-bit Logic Unit Module
// ============================================================================

module logic_unit_4bit (
    input  [3:0] a,
    input  [3:0] b,
    input  [1:0] op,
    output reg [3:0] result
);
    always @(*) begin
        case (op)
            2'b01:   result = a & b;
            2'b10:   result = a | b;
            2'b11:   result = a ^ b;
            default: result = 4'b0000;
        endcase
    end
endmodule

// ============================================================================
// 4-bit Adder Module
// ============================================================================

module adder_4bit (
    input  [3:0] a,
    input  [3:0] b,
    output reg [3:0] sum,
    output reg carry
);
    always @(*) begin
        {carry, sum}  = a + b;
    end
endmodule

// ============================================================================
// Top-Level ALU Module
// ============================================================================

module alu_top (
    input  [3:0] a,
    input  [3:0] b,
    input  [1:0] op,
    output reg [3:0] result,
    output reg carry
);
    wire [3:0] sum_result;
    wire       sum_carry;
    wire [3:0] logic_result;

    // Instantiate adder
    adder_4bit adder_inst (
        .a(a),
        .b(b),
        .sum(sum_result),
        .carry(sum_carry)
    );

    // Instantiate logic unit
    logic_unit_4bit logic_inst (
        .a(a),
        .b(b),
        .op(op),
        .result(logic_result)
    );

    always @(*) begin
        case (op)
            2'b00: begin
                result    = sum_result;
                carry = sum_carry;
            end
            default: begin
                result    = logic_result;
                carry = 1'b0;
            end
        endcase
    end
endmodule

// ============================================================================
// Testbench
// ============================================================================

//`define ENABLE_TB

`ifdef ENABLE_TB

module alu_top_tb;
    reg  [3:0] a;
    reg  [3:0] b;
    reg  [1:0] op;
    wire [3:0] result;
    wire       carry;

    // Instantiate the ALU
    alu_top dut (
        .a(a),
        .b(b),
        .op(op),
        .result(result),
        .carry(carry)
    );

    initial begin
        // Header
        $display("  Time | a    | b    | op  | result  | carry");
        $display("-------+------+-------+-----+--------+-----------");

        // Test all operations
        a = 4'b0011; b = 4'b0101;

        op = 2'b00; #10; // ADD
        $display("%4dns | %b | %b | 00  |  %b   |     %b", $time, a, b, result, carry);

        op = 2'b01; #10; // AND
        $display("%4dns | %b | %b | 01  |  %b   |     %b", $time, a, b, result, carry);

        op = 2'b10; #10; // OR
        $display("%4dns | %b | %b | 10  |  %b   |     %b", $time, a, b, result, carry);

        op = 2'b11; #10; // XOR
        $display("%4dns | %b | %b | 11  |  %b   |     %b", $time, a, b, result, carry);

        // Another input set
        a = 4'b1111; b = 4'b0001;

        op = 2'b00; #10; // ADD
        $display("%4dns | %b | %b | 00  |  %b   |     %b", $time, a, b, result, carry);

        op = 2'b01; #10; // AND
        $display("%4dns | %b | %b | 01  |  %b   |     %b", $time, a, b, result, carry);

        op = 2'b10; #10; // OR
        $display("%4dns | %b | %b | 10  |  %b   |     %b", $time, a, b, result, carry);

        op = 2'b11; #10; // XOR
        $display("%4dns | %b | %b | 11  |  %b   |     %b", $time, a, b, result, carry);

        $finish;
    end
endmodule

`endif

`timescale 1ns / 1ps

module instruction_decoder (
    input  wire [7:0] instruction,
    output wire [1:0] opcode,
    output wire [2:0] rd,
    output wire [2:0] imm
);
    // Slice fields from the instruction
    assign opcode = instruction[7:6];
    assign rd     = instruction[5:3];
    assign imm    = instruction[2:0];
endmodule

// ============================================================================
// Testbench
// ============================================================================

//`define ENABLE_TB

`ifdef ENABLE_TB

module instruction_decoder_tb;
    reg  [7:0] instruction;
    wire [1:0] opcode;
    wire [2:0] rd;
    wire [2:0] imm;

    // Instantiate the decoder
    instruction_decoder dut (
        .instruction(instruction),
        .opcode(opcode),
        .rd(rd),
        .imm(imm)
    );

    initial begin
        $display("Time | instruction | opcode |  rd  | imm ");
        $display("-----+-------------+--------+------+-----");

        // Test case 1: ADDI R1, 5  -> 00 001 101
        instruction = 8'b00001101; #10;
        $display("%4dns |   %b |   %02b   | %03b | %03b", 
                 $time, instruction, opcode, rd, imm);

        // Test case 2: SUBI R3, 6 -> 01 011 110
        instruction = 8'b01011110; #10;
        $display("%4dns |   %b |   %02b   | %03b | %03b", 
                 $time, instruction, opcode, rd, imm);

        // Test case 3: LOAD R0, 2 -> 10 000 010
        instruction = 8'b10000010; #10;
        $display("%4dns |   %b |   %02b   | %03b | %03b", 
                 $time, instruction, opcode, rd, imm);

        // Test case 4: STORE R7, 1 -> 11 111 001
        instruction = 8'b11111001; #10;
        $display("%4dns |   %b |   %02b   | %03b | %03b", 
                 $time, instruction, opcode, rd, imm);

        $finish;
    end
endmodule

`endif

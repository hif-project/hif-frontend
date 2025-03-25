//===========================
// Top-Level ALU Module
//===========================
module alu_top (
    input  [3:0] a,
    input  [3:0] b,
    input  [1:0] op,        // 00 = ADD, 01 = AND, 10 = OR, 11 = XOR
    output reg [3:0] result,
    output reg       carry_out
);
    wire [3:0] sum_result;
    wire       sum_carry;
    wire [3:0] logic_result;

    // Instantiate adder
    adder_4bit adder_inst (
        .a(a),
        .b(b),
        .sum(sum_result),
        .carry_out(sum_carry)
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
                carry_out = sum_carry;
            end
            default: begin
                result    = logic_result;
                carry_out = 1'b0;
            end
        endcase
    end
endmodule

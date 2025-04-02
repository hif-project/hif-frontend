module parser_test #(
    parameter WIDTH = 8,
    parameter DEPTH = 4
) (
    input  wire             clk,
    input  wire             rst,
    input  wire [WIDTH-1:0] data_in,
    output wire [WIDTH-1:0] data_out,
    inout  wire             bidir
);

    // Internal signals
    wire [WIDTH-1:0] bus;
    reg  [WIDTH-1:0] reg_data [0:DEPTH-1];
    reg  [WIDTH-1:0] temp;
    reg  [1:0]       counter;
    reg              valid;
    integer          i;

    // Continuous assignment
    assign data_out = bus;
    assign bus = valid ? temp : {WIDTH{1'bz}};
    assign bidir = valid ? 1'bz : 1'b0;

    // Local parameter
    localparam IDLE = 2'b00,
               RUN  = 2'b01,
               DONE = 2'b10;

    // Initial block
    initial begin
        temp = 0;
        counter = 0;
        valid = 0;
    end

    // Clocked logic
    always @(posedge clk or posedge rst) begin
        if (rst) begin
            counter <= 0;
            valid <= 0;
        end else begin
            case (counter)
                IDLE: begin
                    temp <= data_in;
                    counter <= RUN;
                end
                RUN: begin
                    valid <= 1;
                    counter <= DONE;
                end
                default: begin
                    valid <= 0;
                end
            endcase
        end
    end

    // Combinational logic with for-loop
    always @(*) begin
        for (i = 0; i < DEPTH; i = i + 1) begin
            reg_data[i] = i ^ data_in;
        end
    end

    // Function example
    function [WIDTH-1:0] reverse_bits;
        input [WIDTH-1:0] in;
        integer j;
        begin
            reverse_bits = 0;
            for (j = 0; j < WIDTH; j = j + 1) begin
                reverse_bits[j] = in[WIDTH-1-j];
            end
        end
    endfunction

    // Task example
    //task print_status;
    //    input [WIDTH-1:0] val;
    //    begin
    //        $display("Status: %b", val);
    //    end
    //endtask

    // Generate block
    //genvar g;
    //generate
    //    for (g = 0; g < 2; g = g + 1) begin : GEN_INST
    //        wire dummy = bus[g];
    //    end
    //endgenerate

endmodule

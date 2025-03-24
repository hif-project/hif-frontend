module consumer (
    input wire [7:0] data_in,
    output reg [7:0] stored_data
);
    always @(*) begin
        stored_data = data_in;
    end
endmodule

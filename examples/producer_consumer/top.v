module top;
    wire [7:0] data_bus;
    wire [7:0] result;

    // Instantiate producer
    producer u_producer (
        .data_out(data_bus)
    );

    // Instantiate consumer
    consumer u_consumer (
        .data_in(data_bus),
        .stored_data(result)
    );

    initial begin
        #1; // Small delay to allow assignments to happen
        $display("Result = %b", result);
        $finish;
    end
endmodule

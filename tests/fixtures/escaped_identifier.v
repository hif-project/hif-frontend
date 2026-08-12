// Regression fixture: legal Verilog escaped identifiers (IEEE 1364, backslash
// prefix, terminated by whitespace) currently crash verilog2hif with
// heap corruption ("free(): invalid pointer"), confirmed via
// AddressSanitizer-free minimal repro. This exact syntax is used throughout
// the EPFL combinational benchmark suite (one escaped identifier per bus
// bit), which makes the whole suite unusable with the current frontend.
module top (
    \a[0] , \a[1] , \f[0]
);
    input \a[0] ;
    input \a[1] ;
    output \f[0] ;
    assign \f[0]  = \a[0]  & \a[1] ;
endmodule

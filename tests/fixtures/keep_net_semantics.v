// Nets whose continuous assigns carry net semantics, and one that does not.

// Two continuous drivers on one output: the net is resolved (hif-backend#88).
module two_drivers(input a_en, input [3:0] a, input b_en, input [3:0] b, output [3:0] bus);
  assign bus = a_en ? a : 4'bzzzz;
  assign bus = b_en ? b : 4'bzzzz;
endmodule

// An inout the design drives and also reads: readers must see the pin (hif-backend#89).
module read_back(input [3:0] d, input oe, inout [3:0] bus, output [3:0] sensed);
  assign bus    = oe ? d : 4'bzzzz;
  assign sensed = bus;
endmodule

// An output port with bits nothing drives, which stay z (hif-backend#104).
module partial_port(input [3:0] din, output wire [3:0] dout);
  assign dout[0] = din[0];
  assign dout[2] = din[2];
endmodule

// The same on an internal net, which goes through the cone path, not fixOutputPorts.
module partial_wire(input [3:0] din, output [3:0] y);
  wire [3:0] w;
  assign w[0] = din[0];
  assign w[2] = din[2];
  assign y    = w;
endmodule

// Control: one whole driver, an output nobody reads. Nothing here carries net
// semantics, so it is lowered as it always was.
module single_driver(input [3:0] a, output [3:0] y);
  assign y = ~a;
endmodule

// Control: every bit driven exactly once, by separate assigns. Disjoint, so not
// a multi-driver net, and not partial.
module per_bit(input [3:0] a, output [3:0] y);
  assign y[0] = a[3];
  assign y[1] = a[2];
  assign y[2] = a[1];
  assign y[3] = a[0];
endmodule

// The same partial drive, written as a for generate: once expanded the index is a bit
// string, not an integer (hif-backend#104).
module partial_generate(input [3:0] a, output [3:0] y);
  genvar g;
  generate
    for (g = 0; g < 3; g = g + 1) begin : gg
      assign y[g] = a[g];
    end
  endgenerate
endmodule

// A parameter as an index: bit K, which is 1, is driven twice (hif-backend#88).
module parameter_index #(parameter K = 1) (input ae, input be, input a, input b, output [3:0] bs);
  assign bs[K] = ae ? a : 1'bz;
  assign bs[1] = be ? b : 1'bz;
  assign bs[0] = a;
  assign bs[2] = b;
  assign bs[3] = a & b;
endmodule

// A bit select past the end of the net writes nothing. Bit 3 is never driven, and the
// out-of-range y[9] must not be counted as covering it.
module out_of_range_driver(input [3:0] a, output [3:0] y);
  assign y[0] = a[0];
  assign y[1] = a[1];
  assign y[2] = a[2];
  assign y[9] = a[3];
endmodule

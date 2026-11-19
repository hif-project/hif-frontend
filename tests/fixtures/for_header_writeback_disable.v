// A module-level integer used as a loop index inside a named block that is left
// early with `disable`, then read after the block. The shadowed index has to be
// published after the block: a write-back placed inside it is skipped by the
// disable and `idx` would see a stale `i`.
module for_header_writeback_disable(input [7:0] d, output reg [2:0] idx);
  integer i;

  always @(*) begin
    begin : scan
      for (i = 0; i < 8; i = i + 1) begin
        if (d[i]) disable scan;
      end
    end
    idx = i[2:0];
  end
endmodule

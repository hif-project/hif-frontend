// Regression fixture for hif-frontend#31: a signal shadowed into a `_sig_var`
// lost its write-back when written through a task's out/inout argument.
//
// `refineToVariables` shadows a signal that is both read as a signal and
// written as a variable, renaming every reference to the shadow and inserting
// `sig <= var` after each assignment. A task call that writes the signal
// through an `out`/`inout` argument is also a write, but `fillInfoMap`
// classified it by context rather than by the formal's direction and put it in
// `readUsing` - so the reference was renamed and no write-back followed. The
// signal then stops updating and every reader freezes at whatever the last
// direct assignment left.
//
// Silent: exit 0, output that compiles under iverilog and reparses cleanly, and
// a register that has simply stopped counting.
//
// Four shapes, because the defect needs a specific combination and the
// neighbours have to stay put:
//
//   mixed_out    direct assignment AND a task `output` argument -> the bug
//   mixed_inout  the same with `inout`, which reads as well as writes
//   mixed_part   the same where the actual is a part-select, so the write-back
//                has to carry the part-select and not clobber the whole signal
//   call_only    written ONLY through the task argument. No shadow is created
//                at all here, and none should be: this case was already correct
//                and its translation must not change.
//
// Every signal has its own name so the write-backs can be counted per shape.
//
// iverilog -g2005 accepts this file.
module task_out_arg_writeback (input clk, input rst, output [3:0] a, output [3:0] b, output [7:0] c, output [3:0] d);

  reg [3:0] acc_out;
  reg [3:0] acc_inout;
  reg [7:0] acc_part;
  reg [3:0] acc_call;

  task setnext;   output [3:0] o; begin o = acc_out + 4'd1;   end endtask
  task bump;      inout  [3:0] io; begin io = io + 4'd1;      end endtask
  task sethigh;   output [3:0] o; begin o = 4'd5;             end endtask
  task setplain;  output [3:0] o; begin o = 4'd7;             end endtask

  always @(posedge clk) begin
    if (rst) acc_out = 4'd0;
    else     setnext(acc_out);
  end

  always @(posedge clk) begin
    if (rst) acc_inout = 4'd0;
    else     bump(acc_inout);
  end

  always @(posedge clk) begin
    if (rst) acc_part = 8'd0;
    else     sethigh(acc_part[7:4]);
  end

  always @(posedge clk) setplain(acc_call);

  assign a = acc_out;
  assign b = acc_inout;
  assign c = acc_part;
  assign d = acc_call;
endmodule

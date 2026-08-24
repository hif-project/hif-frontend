// Regression fixture for hif-frontend#25: verilog2hif refused a task's `inout`
// argument, and every ANSI-style task header.
//
// Two separate holes in the same grammar area, so the fixture carries both, and
// carries the forms that already worked alongside them as controls - the point
// is that all six cells of the matrix parse, not that the two new ones do.
//
//   non-ANSI  input / output   already worked
//   non-ANSI  inout            was a bare yyerror
//   ANSI      input / output / inout
//                              the whole form was a bare yyerror
//
// Also covers the two variants that are easy to leave behind when filling in a
// production by copying its twin:
//
//   `inout integer n;`         the task_port_type alternative, a second
//                              yyerror separate from the range_opt one
//   `task t(output p, q);`     the shorthand where a bare identifier continues
//                              the previous item's direction and type
//
// Every task is called, so the parameters have to resolve as well as parse: a
// dropped argument leaves the call with no matching candidate and aborts in
// hif-core's declaration lookup rather than here. That is not hypothetical -
// it is exactly what hif-frontend#27 does to the *function* spelling of the
// shorthand, which this fixture deliberately does not use.
//
// iverilog -g2005 accepts this file.
module task_argument_directions (
    input  wire a,
    output reg  y,
    output reg  z
);

    reg  flag;
    integer count;

    // -- non-ANSI ------------------------------------------------------------

    task na_in;
        input v;
        begin
            y = v;
        end
    endtask

    task na_out;
        output s;
        begin
            s = 1'b1;
        end
    endtask

    task na_inout;
        inout s;
        begin
            s = ~s;
        end
    endtask

    task na_inout_typed;
        inout integer n;
        begin
            n = n + 1;
        end
    endtask

    // -- ANSI ----------------------------------------------------------------

    task ansi_in(input v);
        begin
            y = v;
        end
    endtask

    task ansi_out(output s);
        begin
            s = 1'b1;
        end
    endtask

    task ansi_inout(inout s);
        begin
            s = ~s;
        end
    endtask

    task ansi_mixed(input v, output s, inout io);
        begin
            s  = v;
            io = ~io;
        end
    endtask

    task ansi_shorthand(output p, q);
        begin
            p = 1'b1;
            q = 1'b0;
        end
    endtask

    always @(a) begin
        na_in(a);
        na_out(y);
        flag = a;
        na_inout(flag);
        count = 0;
        na_inout_typed(count);

        ansi_in(a);
        ansi_out(y);
        ansi_inout(flag);
        ansi_mixed(a, y, flag);
        ansi_shorthand(y, z);
    end

endmodule

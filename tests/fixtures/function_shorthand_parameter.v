// Regression fixture for hif-frontend#27: in an ANSI-style function header, a
// parameter written in the shorthand form - a bare identifier continuing the
// previous item's direction and type, `function f(input p, q);` - was dropped.
//
// The `function_port_list K_COMMA IDENTIFIER` production built the port, renamed
// it, and never pushed it onto the list. `task_port_list`'s equivalent, written
// for hif-frontend#25, does push - so the two spellings disagreed, and "tasks
// handle it" was not evidence that functions did. `shorthand_task` below is that
// control, in this fixture so the pair cannot drift apart again.
//
// The defect has two faces, and the fixture carries both because only one of
// them is loud:
//
//   called      the function ends up with fewer parameters than the call site
//               passes, no candidate matches, and hif-core's declaration lookup
//               aborts - exit 134, no artifact.
//   uncalled    nothing resolves anything, so the tool exits 0 and emits a
//               function with the wrong signature. Silent. The issue's severity
//               note said nothing was silently mistranslated; that is the case
//               it missed, and `uncalled_shorthand` pins it.
//
// `typed_shorthand` is the one that separates "a parameter appears" from "the
// right parameter appears": the shorthand has to inherit the previous item's
// *type* as well as its direction, so `q` must come out `[3:0]` and not a bit.
//
// iverilog -g2005 accepts this file.
module function_shorthand_parameter (
    input  wire       a,
    input  wire       b,
    input  wire       c,
    input  wire [3:0] wa,
    input  wire [3:0] wb,
    output wire       y_short,
    output wire       y_mixed,
    output wire       y_long,
    output wire [3:0] y_typed,
    output reg        y_task
);

  // The reported shape: two parameters, the second in shorthand.
  function shorthand_two(input p, q);
    begin shorthand_two = p & q; end
  endfunction

  // Shorthand continuing after an explicit item, then an explicit one again.
  // A fix that pushed only the last shorthand of a run would pass the case
  // above and fail here.
  function mixed_three(input p, q, input r);
    begin mixed_three = p & q & r; end
  endfunction

  // The long form, which always worked. Present so that breaking it fails here
  // rather than looking like a shorthand problem.
  function long_two(input p, input q);
    begin long_two = p & q; end
  endfunction

  // The shorthand must inherit the type, not just the direction. Distinct
  // parameter names so the assertion on `wq`'s type cannot match some other
  // function's `q`.
  function [3:0] typed_shorthand(input [3:0] wp, wq);
    begin typed_shorthand = wp & wq; end
  endfunction

  // Never called, so nothing forces its signature to resolve. Before the fix
  // this translated at exit 0 with one parameter instead of two.
  function uncalled_shorthand(input p, q);
    begin uncalled_shorthand = p | q; end
  endfunction

  // The task spelling of the same shorthand, which already worked
  // (hif-frontend#25). Control.
  task shorthand_task(input p, q, output s);
    begin s = p ^ q; end
  endtask

  assign y_short = shorthand_two(a, b);
  assign y_mixed = mixed_three(a, b, c);
  assign y_long  = long_two(a, b);
  assign y_typed = typed_shorthand(wa, wb);

  always @(a or b) shorthand_task(a, b, y_task);

endmodule

# -----------------------------------------------------------------------------
# @brief  : Tests cmake file.
# @author : Enrico Fraccaroli
# -----------------------------------------------------------------------------

# Scratch directory for generated HIF output, kept out of the fixtures dir.
set(P0_TEST_WORK_DIR ${CMAKE_CURRENT_BINARY_DIR}/p0_work)
file(MAKE_DIRECTORY ${P0_TEST_WORK_DIR})

# Regression: legal escaped identifiers currently crash verilog2hif with
# heap corruption ("free(): invalid pointer"). See fixtures/escaped_identifier.v.
add_test(
    NAME escaped_identifier
    COMMAND $<TARGET_FILE:verilog2hif> -o ${P0_TEST_WORK_DIR}/escaped_identifier
        ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/escaped_identifier.v
)

# Regression: Verilog gate primitives (and/nand/or/...) are not supported at
# all, named or unnamed. See fixtures/primitive_gate.v.
add_test(
    NAME primitive_gate
    COMMAND $<TARGET_FILE:verilog2hif> -o ${P0_TEST_WORK_DIR}/primitive_gate
        ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/primitive_gate.v
)

# Regression: does the escaped-identifier crash depend on file order in a
# multi-file invocation, i.e. does parser state leak across translation
# units within a single run? Both orders currently crash identically
# (confirmed empirically before this test was written), which is itself
# useful signal: the bug is self-contained to parsing that one file's
# content, not caused by state left over from a previous file.
add_test(
    NAME multi_file_order_clean_then_escaped
    COMMAND $<TARGET_FILE:verilog2hif> -o ${P0_TEST_WORK_DIR}/multi_order1
        ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/multi_file_clean.v
        ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/escaped_identifier.v
)
add_test(
    NAME multi_file_order_escaped_then_clean
    COMMAND $<TARGET_FILE:verilog2hif> -o ${P0_TEST_WORK_DIR}/multi_order2
        ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/escaped_identifier.v
        ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/multi_file_clean.v
)

# Regression: the ~& (reduction-NAND) operator is not supported - the grammar
# has a stub production (K_NAND ... primary) that unconditionally calls
# yyerror. See fixtures/reduction_nand.v.
add_test(
    NAME reduction_nand
    COMMAND $<TARGET_FILE:verilog2hif> -o ${P0_TEST_WORK_DIR}/reduction_nand
        ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/reduction_nand.v
)

# Regression: two continuous assigns to different bit-selects of the same
# signal crash post-parsing refinement step 3 - this is the exact
# textbook case splitLogicConesLoops's own doc comment describes handling
# ("a[0] = expr1; a[1] = expr2; --> ..."), so this function has apparently
# never worked for its own worked example. Root cause: it calls
# hif::getTerminalPrefix() with default options, which (despite the
# header's doc-comment claiming recurseIntoMembers/recurseIntoSlices
# default to true) actually default to false since the struct has no
# member initializers - so it returns the Member/Slice node itself
# instead of the underlying Identifier, and getDeclaration() on that
# non-symbol object asserts. See fixtures/multiple_bitselect_assigns.v.
add_test(
    NAME multiple_bitselect_assigns
    COMMAND $<TARGET_FILE:verilog2hif> -o ${P0_TEST_WORK_DIR}/multiple_bitselect_assigns
        ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/multiple_bitselect_assigns.v
)

# Regression: a $-system-function call ($clog2) used in a port range crashed
# post-parsing refinement (no declaration registered for it in hif-core's
# Verilog standard library) and, once that was fixed, crashed again during
# standardization to HIF semantics (no cross-semantics symbol mapping
# entry). See fixtures/system_function_call.v. Fixed in hif-core, not here;
# this fixture exists because the observable regression is here.
add_test(
    NAME system_function_call
    COMMAND $<TARGET_FILE:verilog2hif> -o ${P0_TEST_WORK_DIR}/system_function_call
        ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/system_function_call.v
)

# Regression (hif-frontend#14): verilog2hif aborted with "Declaration not
# found" on a user-defined function called from a continuous assignment to an
# output port - the assignment was folded onto the Entity, where the function,
# a Contents declaration, could not be resolved. A plain `localparam` failed
# identically, so the defect was never about functions.
#
# Driven by a script rather than being a bare exit-0 run: a fold that quietly
# dropped the assignment would also exit 0, so the translation has to be
# checked for being right, not merely for happening.
add_test(
    NAME user_function_continuous_assign
    COMMAND ${CMAKE_COMMAND}
        -DVERILOG2HIF_EXECUTABLE=$<TARGET_FILE:verilog2hif>
        -DFUNCTION_FIXTURE=${CMAKE_CURRENT_SOURCE_DIR}/fixtures/user_function_continuous_assign.v
        -DLOCALPARAM_FIXTURE=${CMAKE_CURRENT_SOURCE_DIR}/fixtures/localparam_continuous_assign.v
        -DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/user_function_continuous_assign_work
        -P ${CMAKE_CURRENT_SOURCE_DIR}/user_function_continuous_assign_test.cmake
)

# Regression (hif-frontend#15): verilog2hif aborted on a replication whose
# repeat count is a $clog2 call, e.g. `{$clog2(DEPTH){1'b1}}`. Fixed in
# hif-core, not here; this fixture exists because the observable regression is
# here.
#
# Driven by a script rather than being a bare exit-0 run: the point of this
# path is that a *symbolic* replication count survives, and a fix that folded
# the count away would also exit 0.
add_test(
    NAME clog2_replication
    COMMAND ${CMAKE_COMMAND}
        -DVERILOG2HIF_EXECUTABLE=$<TARGET_FILE:verilog2hif>
        -DFIXTURE_DIR=${CMAKE_CURRENT_SOURCE_DIR}/fixtures
        -DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/clog2_replication_work
        -P ${CMAKE_CURRENT_SOURCE_DIR}/clog2_replication_test.cmake
)

# Regression: verilog2hif aborted on the SystemVerilog `logic` type, which was
# mistaken for the Verilog-AMS `logic` discipline and renamed to an `ams_logic`
# typedef that a plain Verilog input never declares.
#
# Driven by a script rather than being a bare exit-0 run: `logic` and `reg`
# spell the same four-state variable, so the fix has to make them agree on the
# uninitialised value too ('X', not 'Z'), and it must not disturb the AMS
# reading of the same identifier.
add_test(
    NAME sv_logic_type
    COMMAND ${CMAKE_COMMAND}
        -DVERILOG2HIF_EXECUTABLE=$<TARGET_FILE:verilog2hif>
        -DFIXTURE_DIR=${CMAKE_CURRENT_SOURCE_DIR}/fixtures
        -DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/sv_logic_type_work
        -P ${CMAKE_CURRENT_SOURCE_DIR}/sv_logic_type_test.cmake
)

# Regression: the same type in an ANSI-style port header, which the AMS
# discipline rule refused at parse time even after sv_logic_type made the body
# declaration work. Driven by a script for the same reason: each fixture is
# compared against an already-supported twin that means the same thing, so a
# header that parses but produces a different type or default value fails.
add_test(
    NAME sv_logic_ansi_port
    COMMAND ${CMAKE_COMMAND}
        -DVERILOG2HIF_EXECUTABLE=$<TARGET_FILE:verilog2hif>
        -DFIXTURE_DIR=${CMAKE_CURRENT_SOURCE_DIR}/fixtures
        -DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/sv_logic_ansi_port_work
        -P ${CMAKE_CURRENT_SOURCE_DIR}/sv_logic_ansi_port_test.cmake
)

# Regression: verilog2hif accepted a replication whose repeat count is not a
# constant expression (`{$random{1'b1}}`) and re-emitted it verbatim, so the
# toolchain round-tripped a design no simulator will elaborate. A second illegal
# spelling (`{$time{1'b1}}`) reached hif-core and aborted there instead.
#
# Driven by a script rather than being an expect-failure run: the check must
# reject with a diagnostic that names the offending count and leaves no
# artifact, and it must not touch the legal symbolic counts - parameter,
# localparam, user constant function, genvar - which is the half a bare
# exit-nonzero assertion would miss.
add_test(
    NAME nonconst_replication_count
    COMMAND ${CMAKE_COMMAND}
        -DVERILOG2HIF_EXECUTABLE=$<TARGET_FILE:verilog2hif>
        -DFIXTURE_DIR=${CMAKE_CURRENT_SOURCE_DIR}/fixtures
        -DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/nonconst_replication_count_work
        -P ${CMAKE_CURRENT_SOURCE_DIR}/nonconst_replication_count_test.cmake
)

# Regression (hif-frontend#25): verilog2hif refused a task's `inout` argument
# and every ANSI-style task header. Two bare-yyerror holes in the same grammar
# area - tf_inout_declaration_identifiers, and the whole task_port_item /
# task_port_list family together with the singular tf_output_declaration and
# tf_inout_declaration it needs.
#
# Driven by a script rather than being a bare exit-0 run: filling in a
# production by copying its twin is exactly the operation that gets a direction
# wrong, and a task parsed as `output` where the source said `inout` would exit
# 0 here and only surface in a backend. The script counts the directions and
# names the two spellings easiest to leave behind - `inout integer n;` and the
# `task t(output p, q);` shorthand.
add_test(
    NAME task_argument_directions
    COMMAND ${CMAKE_COMMAND}
        -DVERILOG2HIF_EXECUTABLE=$<TARGET_FILE:verilog2hif>
        -DFIXTURE=${CMAKE_CURRENT_SOURCE_DIR}/fixtures/task_argument_directions.v
        -DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/task_argument_directions_work
        -P ${CMAKE_CURRENT_SOURCE_DIR}/task_argument_directions_test.cmake
)

# Regression (hif-frontend#23): verilog2hif aborted on a continuous assignment
# inside a `for generate` reading the genvar. splitLogicConesLoops declared the
# split signal beside its target - module scope - and generateConeFunctions then
# copied the assignment into a procedure there, out of the genvar's scope.
#
# Driven by a script rather than being a bare exit-0 run: what makes the
# reference resolve is *where* the split declaration lands, and a change that
# silenced the abort while putting it back at module scope would also
# reintroduce the silent half - one signal shared by every iteration of the
# unrolled loop.
add_test(
    NAME genvar_continuous_assign
    COMMAND ${CMAKE_COMMAND}
        -DVERILOG2HIF_EXECUTABLE=$<TARGET_FILE:verilog2hif>
        -DFIXTURE=${CMAKE_CURRENT_SOURCE_DIR}/fixtures/genvar_continuous_assign.v
        -DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/genvar_continuous_assign_work
        -P ${CMAKE_CURRENT_SOURCE_DIR}/genvar_continuous_assign_test.cmake
)

# The forms of `generate` that already worked, kept as a separate test so that
# breaking one of them fails under its own name rather than widening the fix
# above. Covers the two the fix must not touch - an if generate, and a
# continuous assign inside a for generate that does not read the genvar - plus
# the procedural and instance forms from the issue's acceptance matrix.
add_test(
    NAME genvar_generate_controls
    COMMAND $<TARGET_FILE:verilog2hif> -o ${P0_TEST_WORK_DIR}/genvar_generate_controls
        ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/genvar_continuous_assign_controls.v
)

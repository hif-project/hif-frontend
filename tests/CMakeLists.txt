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

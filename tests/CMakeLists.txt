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

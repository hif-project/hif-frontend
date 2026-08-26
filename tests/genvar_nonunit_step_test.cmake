# -----------------------------------------------------------------------------
# @brief  : Regression for hif-frontend#30, strided form: a `for generate` whose
#           index advances by more than one, driving a net read from outside the
#           loop, must be expanded over the values the genvar actually takes.
#
#           While hif-core#24 was open this loop was deliberately left
#           unexpanded, because expansion substituted the iteration ordinal and
#           would have turned bits 0, 2, 4 into bits 0, 1, 2 silently. The
#           earlier version of this test asserted that restriction. hif-core#24
#           is fixed (hif-core 5603a2a), so the assertion is inverted: the loop
#           is expanded, and it is expanded correctly.
#
#           Two independent things are checked, because either can fail alone:
#           the tool no longer aborts (it exits 134 before this change), and the
#           iterations carry the genvar's values rather than its ordinals.
# @author : Enrico Fraccaroli
# -----------------------------------------------------------------------------

foreach(required VERILOG2HIF_EXECUTABLE FIXTURE WORK_DIR)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "Missing required variable: ${required}")
    endif()
endforeach()

file(REMOVE_RECURSE ${WORK_DIR})
file(MAKE_DIRECTORY ${WORK_DIR})

execute_process(
    COMMAND ${VERILOG2HIF_EXECUTABLE} -o genvar_nonunit_step ${FIXTURE}
    WORKING_DIRECTORY ${WORK_DIR}
    RESULT_VARIABLE result
    OUTPUT_VARIABLE tool_output
    ERROR_VARIABLE tool_output
)
if(NOT result EQUAL 0)
    message(FATAL_ERROR
        "verilog2hif failed with exit code ${result}. A net driven from inside a strided "
        "`for generate` and read outside it aborts in generateConeFunctions unless the loop is "
        "expanded first (hif-frontend#30).\n${tool_output}")
endif()

set(HIF_FILE ${WORK_DIR}/genvar_nonunit_step.hif.xml)
if(NOT EXISTS ${HIF_FILE})
    message(FATAL_ERROR "Expected HIF file not produced: ${HIF_FILE}")
endif()

file(READ ${HIF_FILE} hif_content)

# The loop must have been expanded, or the per-iteration assertions below are
# reading tokens of one loop body rather than iterations.
string(FIND "${hif_content}" "<FORGENERATE" fg_at)
if(NOT fg_at EQUAL -1)
    message(FATAL_ERROR
        "The translation still contains a ForGenerate, so the strided loop was not expanded and "
        "the cone for `w` cannot have been built from resolved names (hif-frontend#30).")
endif()

# No reference to the loop index may survive: a copy of the assignment lands in
# a procedure at module scope, where `g` is not declared.
if(hif_content MATCHES "<IDENTIFIER[^>]*name=\"g\"")
    message(FATAL_ERROR
        "A reference to the genvar `g` survives in the translated design (hif-frontend#23).\n"
        "Full content:\n${hif_content}")
endif()

# One split signal per iteration, each named for the bit that iteration drives.
# This is the hif-core#24 assertion: the loop runs over 0, 2, 4, so a suffix of
# 1 or 3 means the iteration ordinal was substituted for the genvar and the
# design now drives bits it does not drive in the source.
string(REGEX MATCHALL "name=\"w_partial[0-9]+_[0-9]+_([0-9]+)\"" partial_names "${hif_content}")
if(NOT partial_names)
    message(FATAL_ERROR
        "No split signal was generated for the bit-select target, so the code path this test "
        "covers did not run (hif-frontend#23).\nFull content:\n${hif_content}")
endif()
list(REMOVE_DUPLICATES partial_names)

set(driven_bits "")
foreach(name IN LISTS partial_names)
    string(REGEX REPLACE "^name=\"w_partial[0-9]+_[0-9]+_([0-9]+)\"$" "\\1" bit "${name}")
    list(APPEND driven_bits ${bit})
endforeach()
list(SORT driven_bits COMPARE NATURAL)

if(NOT "${driven_bits}" STREQUAL "0;2;4")
    message(FATAL_ERROR
        "The expanded loop drives bits ${driven_bits}, but the source drives bits 0, 2 and 4. "
        "Bits 0;1;2 mean the iteration ordinal was substituted for the genvar instead of the "
        "value it takes - a different design, at exit 0, with no diagnostic (hif-core#24).\n"
        "Full content:\n${hif_content}")
endif()

message(STATUS "genvar_nonunit_step test passed.")

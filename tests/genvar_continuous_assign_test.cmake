# -----------------------------------------------------------------------------
# @brief  : Regression (hif-frontend#23): verilog2hif aborted on a continuous
#           assignment inside a `for generate` whose right-hand side or target
#           index reads the genvar.
#
#             [HIF] [getDeclaration] - ERROR: Declaration not found
#             - Raised by hif-core/src/semantics/declarationUtils.cpp:1658
#             - Source file info - g_min.v: line 5, column 17
#               -- in SubProgram: hif_cone_y_partial0_0_0
#
#           splitLogicConesLoops replaces a bit-select target with a fresh
#           signal, and generateConeFunctions later copies the assignment into a
#           procedure declared beside that signal. The signal was declared
#           beside its *target* - module scope - so the copy of the assignment
#           left the genvar's scope, and standardization aborted on the
#           reference at "STD 01: Simplifying source tree".
#
#           REWRITTEN for hif-frontend#30. This test used to assert that the
#           split declaration and its cone landed *inside* the `<FORGENERATE>`
#           element, and it deliberately refused to pass if the loop was ever
#           unrolled earlier - saying it needed rewriting rather than deleting.
#           #30 is that change: `for generate` loops are now expanded before the
#           logic-cone passes, because the same scope mismatch also reaches a
#           cone that has to stay at module scope, where moving the declaration
#           cannot fix it.
#
#           The position check is therefore gone, but the property it stood for
#           is not, and it is now checked directly instead of by proxy. What
#           made #23 dangerous was never the declaration's offset in the file:
#           it was that ONE split signal outside the loop is shared by EVERY
#           iteration, so all of them drive the same net - a defect with no
#           diagnostic at all. After expansion that is visible as a count. Two
#           iterations must produce two distinct split signals. One would be the
#           shared-net bug, back again.
#
#           The genvar assertion is the other half, and it is what #23's abort
#           was actually about: no reference to the loop index may survive,
#           because a copy of the assignment ends up in a procedure at module
#           scope where `g` has no declaration.
#
#           No round-trip leg. The regenerated design still cannot be checked
#           against a simulator here, and that is unchanged by #30.
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
    COMMAND ${VERILOG2HIF_EXECUTABLE} -o genvar_continuous_assign ${FIXTURE}
    WORKING_DIRECTORY ${WORK_DIR}
    RESULT_VARIABLE result
    OUTPUT_VARIABLE tool_output
    ERROR_VARIABLE tool_output
)
if(NOT result EQUAL 0)
    message(FATAL_ERROR
        "verilog2hif failed with exit code ${result} -- which is the reported symptom of "
        "hif-frontend#23.\n${tool_output}")
endif()

set(HIF_FILE ${WORK_DIR}/genvar_continuous_assign.hif.xml)
if(NOT EXISTS ${HIF_FILE})
    message(FATAL_ERROR "Expected HIF file not produced: ${HIF_FILE}")
endif()

file(READ ${HIF_FILE} hif_content)

# The loop must have been expanded. If a future change stops expanding it, the
# two assertions below stop meaning what they say - two signals would then be
# two *tokens* of one loop body rather than two iterations - so fail loudly here
# instead of passing on a coincidence.
string(FIND "${hif_content}" "<FORGENERATE" fg_at)
if(NOT fg_at EQUAL -1)
    message(FATAL_ERROR
        "The translation still contains a ForGenerate. hif-frontend#30 expands `for generate` "
        "before the logic-cone passes, and the per-iteration assertions below depend on that "
        "having happened. If expansion was deliberately removed, this test needs rewriting "
        "rather than deleting.")
endif()

# No reference to the loop index may survive expansion. This is the #23 abort
# itself: generateConeFunctions copies the assignment into a procedure at module
# scope, and a surviving `g` there has no declaration.
if(hif_content MATCHES "<IDENTIFIER[^>]*name=\"g\"")
    message(FATAL_ERROR
        "A reference to the genvar `g` survives in the translated design. A copy of the "
        "assignment is placed in a procedure at module scope, where `g` is not declared, and "
        "standardization aborts on it (hif-frontend#23).\nFull content:\n${hif_content}")
endif()

# One split signal per iteration. The fixture's loop runs twice, so exactly two
# must exist: a single shared one is the silent half of #23, where every
# iteration of the unrolled loop drives the same net.
string(REGEX MATCHALL "name=\"y_partial[0-9_]+\"" partial_names "${hif_content}")
if(NOT partial_names)
    message(FATAL_ERROR
        "No split signal was generated for the bit-select target, so the code path this test "
        "covers did not run (hif-frontend#23).\nFull content:\n${hif_content}")
endif()
list(REMOVE_DUPLICATES partial_names)
list(LENGTH partial_names partial_count)
if(NOT partial_count EQUAL 2)
    message(FATAL_ERROR
        "Expected 2 distinct split signals, one per iteration of the expanded loop, but found "
        "${partial_count}: ${partial_names}. One shared signal means every iteration drives the "
        "same net - the half of hif-frontend#23 that produces no diagnostic.")
endif()

# Each iteration must also get its own cone, for the same reason.
string(REGEX MATCHALL "name=\"hif_cone_y_partial[0-9_]+\"" cone_names "${hif_content}")
list(REMOVE_DUPLICATES cone_names)
list(LENGTH cone_names cone_count)
if(NOT cone_count EQUAL 2)
    message(FATAL_ERROR
        "Expected 2 distinct cone procedures, one per split signal, but found ${cone_count}: "
        "${cone_names} (hif-frontend#23).")
endif()

message(STATUS "genvar_continuous_assign test passed.")

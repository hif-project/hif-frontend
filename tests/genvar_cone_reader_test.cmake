# -----------------------------------------------------------------------------
# @brief  : Regression (hif-frontend#30): verilog2hif aborted when a net driven
#           by a continuous assignment inside a `for generate` was read from
#           outside the loop.
#
#             [HIF] [getDeclaration] - ERROR: Declaration not found
#             - Raised by hif-core/src/semantics/declarationUtils.cpp:1658
#               -- in SubProgram: hif_cone_w_0
#
#           Distinct from #23, which is about the cone for a *split* signal.
#           Here the cone is the one for `w`, and it has to stay at module scope
#           because a module-scope process calls it - so #23's remedy, moving the
#           declaration into the loop, cannot apply. The loop is expanded before
#           the logic-cone passes instead.
#
#           Exit 0 is the reported symptom and is checked, but on its own it is
#           weak: a change that dropped the drivers, or produced one shared
#           signal for both iterations, would also exit 0. So the structure is
#           asserted too.
#
#           The three properties, and why each one:
#
#           1. The cone for `w` exists and the reader calls it. If cone
#              generation stopped running for this shape the abort would go away
#              for the wrong reason, and `q` would sample a net nothing updates.
#           2. Every name the cone's body refers to is declared at module scope -
#              checked as the absence of any surviving reference to the genvar,
#              which is the reference that aborted.
#           3. Two iterations produce two distinct split signals. One would mean
#              both iterations drive the same net, which is the silent half of
#              #23 and would be reintroduced by expanding the loop carelessly.
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
    COMMAND ${VERILOG2HIF_EXECUTABLE} -o genvar_cone_reader ${FIXTURE}
    WORKING_DIRECTORY ${WORK_DIR}
    RESULT_VARIABLE result
    OUTPUT_VARIABLE tool_output
    ERROR_VARIABLE tool_output
)
if(NOT result EQUAL 0)
    message(FATAL_ERROR
        "verilog2hif failed with exit code ${result} -- which is the reported symptom of "
        "hif-frontend#30.\n${tool_output}")
endif()

set(HIF_FILE ${WORK_DIR}/genvar_cone_reader.hif.xml)
if(NOT EXISTS ${HIF_FILE})
    message(FATAL_ERROR "Expected HIF file not produced: ${HIF_FILE}")
endif()

file(READ ${HIF_FILE} hif_content)

# 1. The cone exists, and the reader calls it.
if(NOT hif_content MATCHES "<PROCEDURE[^>]*name=\"hif_cone_w_[0-9]+\"")
    message(FATAL_ERROR
        "No cone procedure was generated for `w`, so the code path this test covers did not run. "
        "Without it the process reads a net that nothing updates (hif-frontend#30).\n"
        "Full content:\n${hif_content}")
endif()
if(NOT hif_content MATCHES "<PCALL[^>]*name=\"hif_cone_w_[0-9]+\"")
    message(FATAL_ERROR
        "The cone for `w` is never called. addConesPCalls must insert the call in the process "
        "that reads `w`, otherwise `q` samples a stale net (hif-frontend#30).\n"
        "Full content:\n${hif_content}")
endif()

# 2. Nothing loop-scoped survives. The genvar is the reference that aborted:
#    the cone's body is a copy of the loop's assignments, and at module scope
#    `g` has no declaration.
if(hif_content MATCHES "<IDENTIFIER[^>]*name=\"g\"")
    message(FATAL_ERROR
        "A reference to the genvar `g` survives. The cone for `w` is declared at module scope "
        "and its body is a copy of the loop's assignments, so this is exactly the unresolvable "
        "reference hif-frontend#30 reports.\nFull content:\n${hif_content}")
endif()
if(hif_content MATCHES "<FORGENERATE")
    message(FATAL_ERROR
        "The translation still contains a ForGenerate, so the loop was not expanded and the "
        "assertions above passed for some other reason (hif-frontend#30).")
endif()

# 3. One split signal per iteration, not one shared by both.
string(REGEX MATCHALL "name=\"w_partial[0-9_]+\"" partial_names "${hif_content}")
list(REMOVE_DUPLICATES partial_names)
list(LENGTH partial_names partial_count)
if(NOT partial_count EQUAL 2)
    message(FATAL_ERROR
        "Expected 2 distinct split signals, one per iteration, but found ${partial_count}: "
        "${partial_names}. A single shared signal means both iterations drive the same net "
        "(hif-frontend#23's silent half).")
endif()

message(STATUS "genvar_cone_reader test passed.")

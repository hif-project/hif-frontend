# -----------------------------------------------------------------------------
# @brief  : Control for hif-frontend#30: a `for generate` with a non-unit step
#           must NOT be expanded while hif-core#24 is open.
#
#           #30 expands `for generate` loops before the logic-cone passes.
#           Expansion substitutes the iteration ordinal for the genvar instead of
#           the value the genvar takes, so a loop stepping by 2 over 0..4 comes
#           out driving bits 0, 1 and 2 - a different design, at exit 0, with no
#           diagnostic (hif-core#24). #30 therefore expands only loops whose
#           index advances by exactly one.
#
#           This test pins that restriction. The fixture has no reader outside
#           the loop, so it exits 0 with or without the guard and exit status
#           discriminates nothing; the assertion is that the ForGenerate is still
#           there and still indexed by the genvar. Removing the guard while
#           hif-core#24 is open fails here rather than silently mistranslating.
#
#           When hif-core#24 is fixed, this test should be replaced by one that
#           asserts the expansion produces bits 0, 2 and 4 - not deleted.
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
    message(FATAL_ERROR "verilog2hif failed with exit code ${result}.\n${tool_output}")
endif()

set(HIF_FILE ${WORK_DIR}/genvar_nonunit_step.hif.xml)
if(NOT EXISTS ${HIF_FILE})
    message(FATAL_ERROR "Expected HIF file not produced: ${HIF_FILE}")
endif()

file(READ ${HIF_FILE} hif_content)

if(NOT hif_content MATCHES "<FORGENERATE")
    message(FATAL_ERROR
        "The non-unit-step loop was expanded. Expansion substitutes the iteration ordinal for "
        "the genvar, so this loop now drives bits 0, 1 and 2 instead of 0, 2 and 4 - silently, "
        "at exit 0 (hif-core#24). Restore the unit-step guard in expandForGenerates, or fix "
        "hif-core#24 first and rewrite this test to assert the correct indices.\n"
        "Full content:\n${hif_content}")
endif()

# The loop is still indexed by the genvar rather than by a folded constant.
if(NOT hif_content MATCHES "<IDENTIFIER[^>]*name=\"g\"")
    message(FATAL_ERROR
        "The ForGenerate survives but no reference to the genvar `g` does, so the loop body was "
        "rewritten in some other way. Check what replaced it before relaxing this "
        "(hif-frontend#30, hif-core#24).\nFull content:\n${hif_content}")
endif()

message(STATUS "genvar_nonunit_step test passed.")

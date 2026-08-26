# -----------------------------------------------------------------------------
# @brief  : Regression (hif-frontend#32): elaborating an `if generate` whose
#           losing branch held the only instance of a module never terminated.
#
#           Once the generate condition folds, the losing branch is removed and
#           its leaf is left defined but instantiated by nothing.
#           partialFlattening's `for (;;)` then spins: collectOnAssigns still
#           collects the leaf's view, because it still drives an output port
#           with a continuous assignment, so `needsFlattening` stays true - but
#           there is no instance left to flatten, so the round changes nothing
#           and the next one collects exactly the same set.
#
#           Measured before the fix: identical `views={leaf, top}, viewRefs={}`
#           from the second iteration onwards, flat memory, about 430 iterations
#           a second, indefinitely. It is a stable oscillation, so it never
#           exhausts memory and never stops on its own.
#
#           The failure mode is a hang, so this test carries a TIMEOUT in
#           CMakeLists.txt. Without it a regression would wedge the suite rather
#           than fail it - which is why the timeout is part of the regression
#           and not incidental.
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
    COMMAND ${VERILOG2HIF_EXECUTABLE} -o if_generate_dead_branch_instance ${FIXTURE}
    WORKING_DIRECTORY ${WORK_DIR}
    RESULT_VARIABLE result
    OUTPUT_VARIABLE tool_output
    ERROR_VARIABLE tool_output
)
if(NOT result EQUAL 0)
    message(FATAL_ERROR
        "verilog2hif failed with exit code ${result}.\n${tool_output}")
endif()

set(HIF_FILE ${WORK_DIR}/if_generate_dead_branch_instance.hif.xml)
if(NOT EXISTS ${HIF_FILE})
    message(FATAL_ERROR "Expected HIF file not produced: ${HIF_FILE}")
endif()

file(READ ${HIF_FILE} hif_content)

if(hif_content MATCHES "<IFGENERATE")
    message(FATAL_ERROR
        "An IfGenerate survives, so the losing branch was never removed and this test is not "
        "exercising the non-termination it is about (hif-frontend#32).\n"
        "Full content:\n${hif_content}")
endif()

# The taken branch is `assign y = a & b`. The discarded branch instantiated the
# leaf, so an INSTANCE surviving would mean the wrong branch was kept.
if(hif_content MATCHES "<INSTANCE")
    message(FATAL_ERROR
        "An instance survives. The branch holding it is the one the static condition discards "
        "(hif-frontend#32).\nFull content:\n${hif_content}")
endif()

message(STATUS "if_generate_dead_branch_instance test passed.")

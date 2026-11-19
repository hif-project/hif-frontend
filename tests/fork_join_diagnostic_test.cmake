# -----------------------------------------------------------------------------
# @brief  : Regression (hif-frontend#26): `fork ... join` ended in a bare
#           yyerror, which asserts. A debug build aborted with exit 134 and no
#           message that named the construct, as if the tool had crashed.
#
#           HIF has no node that runs branches concurrently, so the construct is
#           still not supported. What is checked is how that is reported: exit
#           status exactly 1 (an abort by signal is 134), no artifact, and a
#           message that names fork/join and the issue.
# -----------------------------------------------------------------------------

foreach(required VERILOG2HIF_EXECUTABLE FIXTURE WORK_DIR)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "Missing required variable: ${required}")
    endif()
endforeach()

file(REMOVE_RECURSE ${WORK_DIR})
file(MAKE_DIRECTORY ${WORK_DIR})

execute_process(
    COMMAND ${VERILOG2HIF_EXECUTABLE} -o fork_join ${FIXTURE}
    WORKING_DIRECTORY ${WORK_DIR}
    RESULT_VARIABLE result
    OUTPUT_VARIABLE tool_output
    ERROR_VARIABLE tool_output
)

if(NOT result EQUAL 1)
    message(FATAL_ERROR
        "Expected exit status 1 for an unsupported fork/join, got '${result}'. 134 is an abort "
        "from the assert in yyerror (hif-frontend#26).\n${tool_output}")
endif()

if(NOT tool_output MATCHES "fork/join")
    message(FATAL_ERROR "The message does not name fork/join.\n${tool_output}")
endif()
if(NOT tool_output MATCHES "hif-frontend#26")
    message(FATAL_ERROR "The message does not point to hif-frontend#26.\n${tool_output}")
endif()

file(GLOB artifacts ${WORK_DIR}/*.hif.xml)
if(artifacts)
    message(FATAL_ERROR "An artifact was written for an unsupported construct: ${artifacts}")
endif()

message(STATUS "fork_join_diagnostic test passed.")

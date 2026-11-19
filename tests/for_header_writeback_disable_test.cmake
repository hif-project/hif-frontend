# -----------------------------------------------------------------------------
# @brief  : Regression, follow-up to for_header_writeback (hif-backend#80): the
#           write-back of a shadowed loop index must not sit inside a named
#           block the loop can be `disable`d out of.
#
#           verilog2hif wraps a named block in a named do { ... } while (false)
#           and models `disable name` as a Break. The write-back was placed right
#           after the for, which is inside that block: an early disable skips it,
#           and a module-level integer read after the block keeps a stale value.
#
#           So: nothing inside the WHILE writes `i`, and the write-back is the
#           action right after it.
# -----------------------------------------------------------------------------

foreach(required VERILOG2HIF_EXECUTABLE FIXTURE WORK_DIR)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "Missing required variable: ${required}")
    endif()
endforeach()

file(REMOVE_RECURSE ${WORK_DIR})
file(MAKE_DIRECTORY ${WORK_DIR})

execute_process(
    COMMAND ${VERILOG2HIF_EXECUTABLE} -o for_header_writeback_disable ${FIXTURE}
    WORKING_DIRECTORY ${WORK_DIR}
    RESULT_VARIABLE result
    OUTPUT_VARIABLE tool_output
    ERROR_VARIABLE tool_output
)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "verilog2hif failed with exit code ${result}.\n${tool_output}")
endif()

set(HIF_FILE ${WORK_DIR}/for_header_writeback_disable.hif.xml)
if(NOT EXISTS ${HIF_FILE})
    message(FATAL_ERROR "Expected HIF file not produced: ${HIF_FILE}")
endif()
file(READ ${HIF_FILE} hif_content)

set(write_back "<ASSIGN>[^<]*<LEFTHANDSIDE>[^<]*<IDENTIFIER name=\"i\">[^<]*<CODE_INFO[^>]*/>[^<]*</IDENTIFIER>[^<]*</LEFTHANDSIDE>[^<]*<RIGHTHANDSIDE>[^<]*<IDENTIFIER name=\"i_sig_var")

string(REGEX MATCH "<WHILE .*</WHILE>" while_block "${hif_content}")
if(while_block STREQUAL "")
    message(FATAL_ERROR "No named block (WHILE) in the HIF; the fixture no longer exercises this.")
endif()
if(while_block MATCHES "${write_back}")
    message(FATAL_ERROR
        "The write-back of 'i' is inside the named block. `disable scan` skips it, so 'i' is "
        "stale for whatever reads it after the block.")
endif()

if(NOT hif_content MATCHES "</WHILE>[^A]*${write_back}")
    message(FATAL_ERROR "No write-back of 'i' directly after the named block.")
endif()

message(STATUS "for_header_writeback_disable test passed.")

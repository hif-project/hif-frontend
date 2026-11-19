# -----------------------------------------------------------------------------
# @brief  : Regression (hif-backend#80): the write-back of a shadowed loop index
#           was inserted into the header of the `for`.
#
#           A module-level `integer` used as a loop index is read and written
#           inside a combinational block, so `refineToVariables` shadows it into
#           a `_sig_var` and publishes it with `sig <= var` after every
#           assignment to it. For the init and step assignments of a `for` that
#           put a second assign into `initValues` / `stepActions`, which
#           hif2verilog can only print as a comma expression. Verilog has no
#           comma operator, so the regenerated file did not parse.
#
#           The write-back now goes once after the loop. The body reads the
#           shadow, so nothing else needs the signal while the loop runs.
#
#           Counts, not presence: the init and step lists must hold exactly one
#           assign each, and the publishing assign must follow the loop.
#
#           The behavioural half is hif-regression's
#           `combinational/for_loop_integer`.
# -----------------------------------------------------------------------------

foreach(required VERILOG2HIF_EXECUTABLE FIXTURE WORK_DIR)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "Missing required variable: ${required}")
    endif()
endforeach()

file(REMOVE_RECURSE ${WORK_DIR})
file(MAKE_DIRECTORY ${WORK_DIR})

execute_process(
    COMMAND ${VERILOG2HIF_EXECUTABLE} -o for_header_writeback ${FIXTURE}
    WORKING_DIRECTORY ${WORK_DIR}
    RESULT_VARIABLE result
    OUTPUT_VARIABLE tool_output
    ERROR_VARIABLE tool_output
)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "verilog2hif failed with exit code ${result}.\n${tool_output}")
endif()

set(HIF_FILE ${WORK_DIR}/for_header_writeback.hif.xml)
if(NOT EXISTS ${HIF_FILE})
    message(FATAL_ERROR "Expected HIF file not produced: ${HIF_FILE}")
endif()
file(READ ${HIF_FILE} hif_content)

# Non-greedy per block: the fixture holds a single for.
string(REGEX MATCH "<INITVALUES>.*</INITVALUES>" init_block "${hif_content}")
string(REGEX MATCHALL "<ASSIGN" init_assigns "${init_block}")
list(LENGTH init_assigns init_count)
string(REGEX MATCH "<STEPACTIONS>.*</STEPACTIONS>" step_block "${hif_content}")
string(REGEX MATCHALL "<ASSIGN" step_assigns "${step_block}")
list(LENGTH step_assigns step_count)
if(NOT init_count EQUAL 1 OR NOT step_count EQUAL 1)
    message(FATAL_ERROR
        "Expected 1 assign each in the for's init and step lists, found ${init_count} and "
        "${step_count}. More than one is the shadowed index's write-back inside the loop "
        "header, which prints as a comma expression (hif-backend#80).")
endif()

# The write-back exists, and follows the loop: `i <= i_sig_var_N` right after </FOR>.
if(NOT hif_content MATCHES "</FOR>[^A]*<ASSIGN[^>]*>[^A]*<LEFTHANDSIDE>[^<]*<IDENTIFIER name=\"i\">[^<]*<CODE_INFO[^>]*/>[^<]*</IDENTIFIER>[^<]*</LEFTHANDSIDE>[^<]*<RIGHTHANDSIDE>[^<]*<IDENTIFIER name=\"i_sig_var")
    message(FATAL_ERROR
        "No `i <= i_sig_var_N` write-back directly after the for. The index would never be "
        "published (hif-backend#80).")
endif()

message(STATUS "for_header_writeback test passed.")

# -----------------------------------------------------------------------------
# @brief  : Regression (hif-frontend#25): verilog2hif refused a task's `inout`
#           argument, and every ANSI-style task header.
#
#           Two separate holes in the same grammar area, both bare yyerrors
#           rather than something being wrong:
#
#             tf_inout_declaration_identifiers   the non-ANSI `inout s;`, with
#                                                the implementation sitting
#                                                commented out beneath it
#             task_port_item / task_port_list    the whole ANSI form, for every
#                                                direction, plus the singular
#                                                tf_output_declaration and
#                                                tf_inout_declaration it needs
#
#           Both aborted at exit 134 with no artifact:
#
#             -- ERROR: tf_inout_declaration_identifiers: K_inout K_reg_opt
#                K_signed_opt range_opt list_of_identifiers is not supported.
#             verilog2hif: verilog_support.cpp:219: void yyerror(...):
#                Assertion `false' failed.
#
#             -- ERROR: task_port_item: attribute_instance_list
#                tf_input_declaration is not supported.
#
#           Exit 0 is not a sufficient check. Filling in a production by copying
#           its twin is exactly the operation that gets a direction wrong - the
#           commented-out `inout` call carried a stale two-argument signature,
#           so the arguments had to be reworked rather than uncommented - and a
#           task parsed with the wrong direction would exit 0 here and only
#           surface much later, in a backend that emits `output` where the
#           source said `inout`. So the directions are counted, not just the
#           exit code.
#
#           The fixture calls every task, so the parameters have to resolve as
#           well as parse. That is not ceremony: a dropped argument aborts in
#           hif-core's declaration lookup rather than in the parser, which is
#           what hif-frontend#27 does to the function spelling of the shorthand.
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
    COMMAND ${VERILOG2HIF_EXECUTABLE} -o task_argument_directions ${FIXTURE}
    WORKING_DIRECTORY ${WORK_DIR}
    RESULT_VARIABLE result
    OUTPUT_VARIABLE tool_output
    ERROR_VARIABLE tool_output
)
if(NOT result EQUAL 0)
    message(FATAL_ERROR
        "verilog2hif failed with exit code ${result} -- which is the reported symptom of "
        "hif-frontend#25.\n${tool_output}")
endif()

set(HIF_FILE ${WORK_DIR}/task_argument_directions.hif.xml)
if(NOT EXISTS ${HIF_FILE})
    message(FATAL_ERROR "Expected HIF file not produced: ${HIF_FILE}")
endif()

file(READ ${HIF_FILE} hif_content)

# Every task has to be there. A grammar that silently dropped one would still
# satisfy the direction counts below if the losses happened to cancel out.
foreach(task na_in na_out na_inout na_inout_typed
             ansi_in ansi_out ansi_inout ansi_mixed ansi_shorthand)
    if(NOT hif_content MATCHES "<PROCEDURE[^>]*name=\"${task}\"")
        message(FATAL_ERROR
            "The translation contains no procedure named '${task}', so that spelling of a task "
            "header did not survive (hif-frontend#25).")
    endif()
endforeach()

# The directions, counted. Getting one wrong is the failure mode that exits 0.
#
#   IN     na_in.v, ansi_in.v, ansi_mixed.v
#   OUT    na_out.s, ansi_out.s, ansi_mixed.s, ansi_shorthand.p, ansi_shorthand.q
#   INOUT  na_inout.s, na_inout_typed.n, ansi_inout.s, ansi_mixed.io
function(count_direction direction expected)
    string(REGEX MATCHALL "<PARAMETER[^>]*direction=\"${direction}\"" matches "${hif_content}")
    list(LENGTH matches actual)
    if(NOT actual EQUAL expected)
        message(FATAL_ERROR
            "Expected ${expected} task parameters with direction ${direction}, found ${actual}. A "
            "production filled in from its twin has taken the twin's direction "
            "(hif-frontend#25).")
    endif()
endfunction()

count_direction("IN" 3)
count_direction("OUT" 5)
count_direction("INOUT" 4)

# The two spellings that are easiest to leave behind, named explicitly so a
# failure says which one rather than only that a count is off.
if(NOT hif_content MATCHES "<PARAMETER[^>]*direction=\"INOUT\"[^>]*name=\"n\"")
    message(FATAL_ERROR
        "`inout integer n;` did not produce a dir_inout parameter. That is the task_port_type "
        "alternative, a second refusal separate from the range_opt one (hif-frontend#25).")
endif()

foreach(shorthand p q)
    if(NOT hif_content MATCHES "<PARAMETER[^>]*direction=\"OUT\"[^>]*name=\"${shorthand}\"")
        message(FATAL_ERROR
            "`task t(output p, q);` lost parameter '${shorthand}'. A bare identifier continues the "
            "previous item's direction and type, and has to be pushed onto the list as its own "
            "port (hif-frontend#25; the function spelling of this is hif-frontend#27).")
    endif()
endforeach()

message(STATUS "task_argument_directions test passed.")

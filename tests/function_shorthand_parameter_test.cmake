# -----------------------------------------------------------------------------
# @brief  : Regression (hif-frontend#27): in an ANSI-style function header, a
#           parameter written in the shorthand form - a bare identifier
#           continuing the previous item's direction and type,
#           `function f(input p, q);` - was dropped.
#
#           The `function_port_list K_COMMA IDENTIFIER` production built the
#           port, renamed it, and never pushed it onto the list.
#           `task_port_list`'s equivalent, written for hif-frontend#25, does
#           push, so the two spellings disagreed - and "tasks handle it" was not
#           evidence that functions did. The fixture carries the task control for
#           that reason.
#
#           Exit 0 is not sufficient, and the reason is not the usual one. The
#           defect has a loud face and a quiet one:
#
#             called    the function has fewer parameters than the call site
#                       passes, nothing matches, and hif-core's declaration
#                       lookup aborts. Exit status catches this.
#             uncalled  nothing resolves anything, so the tool exits 0 and emits
#                       a function with the wrong signature. Exit status catches
#                       nothing at all.
#
#           So the parameters are counted. `uncalled_shorthand` exists only to
#           be counted, and would pass an exit-status test before the fix.
#
#           The type is checked separately from the direction, because the
#           shorthand inherits both and a fix that pushed a default-typed port
#           would satisfy the counts.
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
    COMMAND ${VERILOG2HIF_EXECUTABLE} -o function_shorthand_parameter ${FIXTURE}
    WORKING_DIRECTORY ${WORK_DIR}
    RESULT_VARIABLE result
    OUTPUT_VARIABLE tool_output
    ERROR_VARIABLE tool_output
)
if(NOT result EQUAL 0)
    message(FATAL_ERROR
        "verilog2hif failed with exit code ${result} -- which is the reported symptom of "
        "hif-frontend#27: the dropped parameter leaves the call with no matching candidate and "
        "hif-core's declaration lookup aborts.\n${tool_output}")
endif()

set(HIF_FILE ${WORK_DIR}/function_shorthand_parameter.hif.xml)
if(NOT EXISTS ${HIF_FILE})
    message(FATAL_ERROR "Expected HIF file not produced: ${HIF_FILE}")
endif()

file(READ ${HIF_FILE} hif_content)

# Every subprogram has to be there before any count means anything.
foreach(fn shorthand_two mixed_three long_two typed_shorthand uncalled_shorthand)
    if(NOT hif_content MATCHES "<FUNCTION[^>]*name=\"${fn}\"")
        message(FATAL_ERROR
            "The translation contains no function named '${fn}' (hif-frontend#27).")
    endif()
endforeach()
if(NOT hif_content MATCHES "<PROCEDURE[^>]*name=\"shorthand_task\"")
    message(FATAL_ERROR
        "The translation contains no procedure named 'shorthand_task'. That is the task spelling "
        "of the same shorthand and it already worked (hif-frontend#25); losing it means the fix "
        "reached further than it should.")
endif()

# The parameters, counted.
#
#   IN   shorthand_two p q | mixed_three p q r | long_two p q
#        typed_shorthand wp wq | uncalled_shorthand p q | shorthand_task p q   = 13
#   OUT  shorthand_task s                                                      = 1
#
# Before the fix each shorthand continuation was missing, which is IN = 9. The
# `uncalled_shorthand` pair is the part no exit code would have caught.
function(count_direction direction expected)
    string(REGEX MATCHALL "<PARAMETER[^>]*direction=\"${direction}\"" matches "${hif_content}")
    list(LENGTH matches actual)
    if(NOT actual EQUAL expected)
        message(FATAL_ERROR
            "Expected ${expected} subprogram parameters with direction ${direction}, found "
            "${actual}. A shorthand continuation that is dropped, or pushed with the wrong "
            "direction, lands here (hif-frontend#27).")
    endif()
endfunction()

count_direction("IN" 13)
count_direction("OUT" 1)

# The shorthand inherits the previous item's type as well as its direction.
# `typed_shorthand(input [3:0] wp, wq)` must give `wq` the vector, not a bit.
if(NOT hif_content MATCHES "<PARAMETER[^>]*name=\"wq\"[^>]*>[ \t\r\n]*<TYPE>[ \t\r\n]*<BITVECTOR")
    message(FATAL_ERROR
        "The shorthand parameter 'wq' is not a BITVECTOR, so it did not inherit the type of the "
        "`input [3:0] wp` it continues. Direction alone is not what the shorthand carries "
        "(hif-frontend#27).\nFull content:\n${hif_content}")
endif()

message(STATUS "function_shorthand_parameter test passed.")

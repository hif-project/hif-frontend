# -----------------------------------------------------------------------------
# @brief  : Regression (hif-frontend#31): a signal shadowed into a `_sig_var`
#           lost its write-back when written through a task's out/inout
#           argument, so the signal stopped updating.
#
#           `fillInfoMap` classified a reference by its syntactic context and
#           never consulted the formal's direction, so an actual bound to an
#           `out`/`inout` formal landed in `readUsing`. `refineToVariables`
#           renames a `readUsing` reference to the shadow and stops there; the
#           `sig <= var` write-back is inserted only in the loop over assignment
#           targets, and a ProcedureCall is not an Assign. Rename without
#           write-back, and the two halves of the shadowing contract come apart.
#
#           Exit status proves nothing here - the defect is silent. The output
#           compiles under iverilog and reparses cleanly; the register has just
#           stopped counting. So the write-backs are counted.
#
#           Counts rather than presence, because before the fix each of these
#           signals already had exactly one write-back - the one following its
#           direct assignment. The missing one is the second. A test asserting
#           only that a write-back exists would have passed throughout.
#
#           `acc_call` is the control and the one that must NOT change: written
#           only through the task argument, it is never shadowed, and that case
#           was already correct. If a future change starts shadowing it, this
#           test says so rather than letting a new write-back appear where none
#           is needed.
#
#           The behavioural half lives in hif-regression's
#           `sequential/verilog_task_arguments`, which is the design that found
#           this and which reads 0 for its whole run against a source that reads
#           3, 6, 9, 12, 15. This repository's suite does not simulate.
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
    COMMAND ${VERILOG2HIF_EXECUTABLE} -o task_out_arg_writeback ${FIXTURE}
    WORKING_DIRECTORY ${WORK_DIR}
    RESULT_VARIABLE result
    OUTPUT_VARIABLE tool_output
    ERROR_VARIABLE tool_output
)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "verilog2hif failed with exit code ${result}.\n${tool_output}")
endif()

set(HIF_FILE ${WORK_DIR}/task_out_arg_writeback.hif.xml)
if(NOT EXISTS ${HIF_FILE})
    message(FATAL_ERROR "Expected HIF file not produced: ${HIF_FILE}")
endif()

file(READ ${HIF_FILE} hif_content)

# `sig <= sig_sig_var_N` where both sides are plain identifiers. Two per signal:
# one after the direct assignment, one after the task call. One means the call's
# is missing, which is hif-frontend#31.
function(count_writebacks signal expected)
    string(REGEX MATCHALL
        "<IDENTIFIER name=\"${signal}\">[^<]*<CODE_INFO[^>]*/>[^<]*</IDENTIFIER>[^<]*</LEFTHANDSIDE>[^<]*<RIGHTHANDSIDE>[^<]*<IDENTIFIER name=\"${signal}_sig_var"
        matches "${hif_content}")
    list(LENGTH matches actual)
    if(NOT actual EQUAL expected)
        message(FATAL_ERROR
            "Expected ${expected} write-backs of the shadow to '${signal}', found ${actual}. "
            "One means the direct assignment got its write-back and the task's out/inout argument "
            "did not, so '${signal}' stops updating (hif-frontend#31).")
    endif()
endfunction()

count_writebacks(acc_out 2)
count_writebacks(acc_inout 2)

# The part-select actual: the write-back has to carry the part-select, not
# clobber the whole signal, so both sides are slices of it.
string(REGEX MATCHALL
    "</LEFTHANDSIDE>[^<]*<RIGHTHANDSIDE>[^<]*<SLICE>[^<]*<PREFIX>[^<]*<IDENTIFIER name=\"acc_part_sig_var"
    part_matches "${hif_content}")
list(LENGTH part_matches part_count)
if(NOT part_count EQUAL 1)
    message(FATAL_ERROR
        "Expected 1 part-select write-back for 'acc_part', found ${part_count}. The task writes "
        "only acc_part[7:4], so the write-back must carry the same part-select rather than "
        "publishing the whole shadow (hif-frontend#31).")
endif()

# The control. Written only through the task argument, so no shadow is created
# and no write-back is needed - and none must appear.
if(hif_content MATCHES "acc_call_sig_var")
    message(FATAL_ERROR
        "'acc_call' was shadowed into a _sig_var. It is written only through the task argument, "
        "never directly, so the mix that triggers shadowing does not occur and this case was "
        "already correct - the fix for hif-frontend#31 must not widen to it.")
endif()

message(STATUS "task_out_arg_writeback test passed.")

# -----------------------------------------------------------------------------
# @brief  : Regression (hif-frontend#20): verilog2hif accepted a replication
#           whose repeat count is not a constant expression, translated it, and
#           hif2verilog re-emitted it verbatim - so the toolchain round-tripped
#           a design no simulator will elaborate.
#
#           IEEE Std 1364-2005, 5.1.14 requires the repeat count to be a
#           constant expression. Two inputs violating it behaved differently and
#           both were wrong: `{$random{1'b1}}` was accepted at exit 0, while
#           `{$time{1'b1}}` reached hif-core's mapStandardSymbols and aborted
#           there with "Expected declaration" - an internal error rather than a
#           diagnostic about the input.
#
#           Exit-nonzero alone is not a sufficient check, in both directions.
#           A check that simply refused every symbolic count would also make
#           these two fixtures fail, while destroying `{N{1'b1}}` - the whole
#           point of the iterated_concat path is that a legal symbolic count
#           survives. So this test pins both halves: the illegal counts are
#           rejected *with the diagnostic naming them*, and the legal ones still
#           translate.
# @author : Enrico Fraccaroli
# -----------------------------------------------------------------------------

foreach(required VERILOG2HIF_EXECUTABLE FIXTURE_DIR WORK_DIR)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "Missing required variable: ${required}")
    endif()
endforeach()

file(REMOVE_RECURSE ${WORK_DIR})
file(MAKE_DIRECTORY ${WORK_DIR})

function(run label out_result out_output)
    execute_process(
        COMMAND ${VERILOG2HIF_EXECUTABLE} -o ${label} ${FIXTURE_DIR}/${label}.v
        WORKING_DIRECTORY ${WORK_DIR}
        RESULT_VARIABLE result
        OUTPUT_VARIABLE tool_output
        ERROR_VARIABLE tool_output
    )
    set(${out_result} "${result}" PARENT_SCOPE)
    set(${out_output} "${tool_output}" PARENT_SCOPE)
endfunction()

# An illegal count must be refused, must say why, and must leave nothing behind:
# a HIF file on disk is what let the invalid design travel downstream.
function(require_rejected label expected_mention)
    run(${label} result output)

    if(result EQUAL 0)
        message(FATAL_ERROR
            "verilog2hif accepted ${label}.v, whose replication count is not a constant expression "
            "(hif-frontend#20). It must be diagnosed and refused.\nOutput:\n${output}")
    endif()

    if(NOT output MATCHES "Replication count is not a constant expression")
        message(FATAL_ERROR
            "${label}.v was refused, but not by the constant-expression check - so the fixture is "
            "failing for the wrong reason and no longer discriminates.\nOutput:\n${output}")
    endif()

    # The diagnostic has to name the offending construct, not just the rule.
    if(NOT output MATCHES "${expected_mention}")
        message(FATAL_ERROR
            "The diagnostic for ${label}.v does not mention '${expected_mention}', so it does not "
            "tell the user which count is invalid.\nOutput:\n${output}")
    endif()

    # File position, so the report is actionable in a real design.
    if(NOT output MATCHES "${label}\\.v: line [0-9]+, column [0-9]+")
        message(FATAL_ERROR
            "The diagnostic for ${label}.v carries no file/line/column.\nOutput:\n${output}")
    endif()

    if(EXISTS ${WORK_DIR}/${label}.hif.xml)
        message(FATAL_ERROR
            "verilog2hif refused ${label}.v but still wrote ${label}.hif.xml. The invalid design "
            "remains available to the rest of the toolchain (hif-frontend#20).")
    endif()
endfunction()

function(require_accepted label)
    run(${label} result output)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR
            "verilog2hif refused ${label}.v with exit code ${result}, but every replication count in "
            "it is a legal constant expression. The check is over-broad.\nOutput:\n${output}")
    endif()
    if(NOT EXISTS ${WORK_DIR}/${label}.hif.xml)
        message(FATAL_ERROR "Expected HIF file not produced: ${WORK_DIR}/${label}.hif.xml")
    endif()
endfunction()

# --------------------------------------------------------------------------
# Case 1: the issue's reproducer. A system function reporting simulation state.
# --------------------------------------------------------------------------
require_rejected(nonconst_replication_system_function "_system_random")

# --------------------------------------------------------------------------
# Case 2: the other route into the same rule - a port read as the count. Caught
# on the declaration kind rather than on the callee name, so it exercises a
# different branch of the check.
# --------------------------------------------------------------------------
require_rejected(nonconst_replication_signal "'s'")

# --------------------------------------------------------------------------
# Case 3: the over-rejection guard. Parameter, expression over a parameter,
# localparam and user constant function are all legal counts.
# --------------------------------------------------------------------------
require_accepted(const_replication_counts)

# The parameterisation must still be symbolic afterwards - a check that folded
# the count away would pass case 3 while destroying what clog2_replication
# protects.
file(READ ${WORK_DIR}/const_replication_counts.hif.xml const_hif)
if(NOT const_hif MATCHES "<FCALL[^>]*name=\"hif_verilog_iterated_concat\"")
    message(FATAL_ERROR
        "No 'hif_verilog_iterated_concat' call survives in const_replication_counts.hif.xml: the "
        "legal symbolic counts were folded or dropped.\nContent:\n${const_hif}")
endif()

# --------------------------------------------------------------------------
# Case 4: a genvar count. Legal (IEEE Std 1364-2005, 12.1.3.2) and modelled as a
# hif::Variable at the point the check runs, so without its explicit exemption
# it would be reported as reading a variable.
#
# This case used to accept a non-zero exit, because the fixture aborted for the
# unrelated reason recorded in hif-frontend#23, and it said to tighten it to
# require_accepted() once that stopped. hif-frontend#30 is when it stopped: the
# `for generate` is now expanded before the logic-cone passes, so the cone no
# longer copies a genvar reference out of the loop. Tightened accordingly - a
# regression to the abort now fails here instead of printing a status line.
#
# The count check itself is still asserted separately below, because exit 0
# alone would not distinguish "the exemption works" from "the tool stopped
# reaching the check".
# --------------------------------------------------------------------------
run(genvar_replication_count genvar_result genvar_output)
if(genvar_output MATCHES "Replication count is not a constant expression")
    message(FATAL_ERROR
        "A genvar replication count was reported as non-constant. A genvar is constant within its "
        "generate loop, so `{g{1'b1}}` is legal and the exemption in _checkReplicationCount is "
        "missing or ineffective.\nOutput:\n${genvar_output}")
endif()
if(NOT genvar_result EQUAL 0)
    message(FATAL_ERROR
        "verilog2hif refused genvar_replication_count.v with exit code ${genvar_result}. It aborted "
        "here until hif-frontend#30 expanded the `for generate` before the logic-cone passes; a "
        "return to that abort is a regression, not the expected state.\nOutput:\n${genvar_output}")
endif()
if(NOT EXISTS ${WORK_DIR}/genvar_replication_count.hif.xml)
    message(FATAL_ERROR "Expected HIF file not produced: ${WORK_DIR}/genvar_replication_count.hif.xml")
endif()

message(STATUS "nonconst_replication_count test passed.")

# -----------------------------------------------------------------------------
# @brief  : Regression (hif-frontend#15): verilog2hif aborted with hif-core's
#           "Expected declaration" on a replication whose repeat count is a
#           $clog2 call, e.g. `{$clog2(DEPTH){1'b1}}`.
#
#           Cause (fixed in hif-core, not here): the getSimplifiedSymbol
#           implementation for `iterated_concat` copies the whole call so the
#           replication count can stay symbolic, but it only mapped the outer
#           call into destination form. The nested `$clog2` stayed spelled
#           `_system_clog2` under a Library the enclosing map step had already
#           renamed to `hif_verilog_standard`, which declares only
#           `hif_verilog__system_clog2` - so nothing under that nested call
#           could resolve.
#
#           Exit 0 is not a sufficient check here. The whole point of the
#           iterated_concat path is that a *symbolic* count survives; a fix that
#           folded the count to a constant, or dropped the $clog2 call, would
#           also exit 0 while destroying the parameterisation. So the produced
#           HIF is required to still contain the call and its DEPTH reference,
#           and to be free of any source-form Verilog spelling.
# @author : Enrico Fraccaroli
# -----------------------------------------------------------------------------

foreach(required VERILOG2HIF_EXECUTABLE FIXTURE_DIR WORK_DIR)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "Missing required variable: ${required}")
    endif()
endforeach()

file(REMOVE_RECURSE ${WORK_DIR})
file(MAKE_DIRECTORY ${WORK_DIR})

function(translate label out_content)
    set(fixture ${FIXTURE_DIR}/${label}.v)
    execute_process(
        COMMAND ${VERILOG2HIF_EXECUTABLE} -o ${label} ${fixture}
        WORKING_DIRECTORY ${WORK_DIR}
        RESULT_VARIABLE result
        OUTPUT_VARIABLE tool_output
        ERROR_VARIABLE tool_output
    )
    if(NOT result EQUAL 0)
        message(FATAL_ERROR
            "verilog2hif failed on ${fixture} with exit code ${result} (hif-frontend#15).\n${tool_output}")
    endif()

    set(hif_file ${WORK_DIR}/${label}.hif.xml)
    if(NOT EXISTS ${hif_file})
        message(FATAL_ERROR "Expected HIF file not produced: ${hif_file}")
    endif()
    file(READ ${hif_file} content)
    if(content STREQUAL "")
        message(FATAL_ERROR "verilog2hif produced an empty HIF file for ${fixture}.")
    endif()
    set(${out_content} "${content}" PARENT_SCOPE)
endfunction()

# Every standard symbol in the produced HIF has to be in destination form. A
# source-form leftover is the defect's signature, and is what stops the tree
# resolving.
function(require_destination_form label content)
    if(content MATCHES "name=\"_system_")
        message(FATAL_ERROR
            "${label}: a source-form '_system_*' call survives in the produced HIF; the subtree is "
            "mixed-domain (hif-frontend#15).\nContent:\n${content}")
    endif()
    if(content MATCHES "<LIBRARY[^>]*name=\"standard\"")
        message(FATAL_ERROR
            "${label}: a source-form Library named 'standard' survives in the produced HIF "
            "(hif-frontend#15).\nContent:\n${content}")
    endif()
    if(content MATCHES "hif_verilog_hif_verilog_")
        message(FATAL_ERROR
            "${label}: a name carries a doubled 'hif_verilog_' prefix, so a canonical spelling was "
            "applied more than once.\nContent:\n${content}")
    endif()
endfunction()

# --------------------------------------------------------------------------
# Case 1: the issue's reproducer.
# --------------------------------------------------------------------------
translate(clog2_replication replication_hif)
require_destination_form("clog2_replication" "${replication_hif}")

if(NOT replication_hif MATCHES "<FCALL[^>]*name=\"hif_verilog_iterated_concat\"")
    message(FATAL_ERROR
        "The replication is gone from the produced HIF: no 'hif_verilog_iterated_concat' call. A "
        "symbolic replication count must survive as a call, not be folded away.\n"
        "Content:\n${replication_hif}")
endif()
if(NOT replication_hif MATCHES "<FCALL[^>]*name=\"hif_verilog__system_clog2\"")
    message(FATAL_ERROR
        "The $clog2 replication count is gone from the produced HIF. It was folded or dropped rather "
        "than kept symbolic (hif-frontend#15).\nContent:\n${replication_hif}")
endif()
if(NOT replication_hif MATCHES "<IDENTIFIER[^>]*name=\"DEPTH\"")
    message(FATAL_ERROR
        "The parameter 'DEPTH' is no longer referenced in the produced HIF, so the replication count "
        "is no longer parameterised.\nContent:\n${replication_hif}")
endif()

# --------------------------------------------------------------------------
# Case 2: the same construct as one element of a larger concatenation.
# --------------------------------------------------------------------------
translate(clog2_replication_in_concat concat_hif)
require_destination_form("clog2_replication_in_concat" "${concat_hif}")

if(NOT concat_hif MATCHES "<FCALL[^>]*name=\"hif_verilog__system_clog2\"")
    message(FATAL_ERROR
        "The $clog2 replication count is gone from the produced HIF for the larger concatenation.\n"
        "Content:\n${concat_hif}")
endif()
if(NOT concat_hif MATCHES "<IDENTIFIER[^>]*name=\"DEPTH\"")
    message(FATAL_ERROR
        "The parameter 'DEPTH' is no longer referenced in the larger concatenation's HIF.\n"
        "Content:\n${concat_hif}")
endif()

# --------------------------------------------------------------------------
# Case 3: shapes that already worked and must keep working. If the fix were
# over-broad - rewriting calls it has no business rewriting, or blocking the
# constant folding a constant count is supposed to get - this is what says so.
# --------------------------------------------------------------------------
translate(replication_baseline baseline_hif)
require_destination_form("replication_baseline" "${baseline_hif}")

foreach(signal a b c d)
    if(NOT baseline_hif MATCHES "name=\"${signal}\"")
        message(FATAL_ERROR
            "Baseline signal '${signal}' is missing from the produced HIF; a replication or "
            "concatenation shape that used to translate no longer does.\nContent:\n${baseline_hif}")
    endif()
endforeach()
if(NOT baseline_hif MATCHES "<IDENTIFIER[^>]*name=\"N\"")
    message(FATAL_ERROR
        "The parameter 'N' is no longer referenced, so `{N{1'b1}}` lost its symbolic count.\n"
        "Content:\n${baseline_hif}")
endif()

# --------------------------------------------------------------------------
# Case 4: $clog2 as a module parameter actual. This was suspected to be a
# second exposure of the same defect and measured not to be - it translated
# before the fix too. Kept as a guard so the suspicion stays settled.
# --------------------------------------------------------------------------
translate(clog2_module_actual actual_hif)
require_destination_form("clog2_module_actual" "${actual_hif}")

message(STATUS "clog2_replication test passed.")

# -----------------------------------------------------------------------------
# @brief  : Regression (hif-muffin#24): the write-back assignment
#           splitLogicConesLoops synthesizes for a part-select continuous
#           assign carried no source attribution.
#
#           `assign y[7:4] = a[3:0]` is rewritten into a split signal
#           `y_partial0_0`, the original assignment retargeted to it, and a new
#           write-back `y[7:4] = y_partial0_0`. The retargeted original keeps
#           its own code info; the write-back was created with none. Every
#           consumer that reads a statement's position off the Assign therefore
#           saw nothing for it - `muffin --list-faults` reported
#           `"source": "", "line": 0`, so two locations on one signal produced
#           records identical in every field but `id` and neither could be
#           selected by attribute. hif-core's diagnostics printed
#           "No source file info available" for the same node.
#
#           Exit 0 is not a sufficient check: the defect never affected exit
#           status, and the tool was already exiting 0 while emitting the
#           unattributed node. Counting is not sufficient either. The assertion
#           that discriminates is that *each* source line appears on *two*
#           assignments - the cone and its write-back - because before the fix
#           each appeared exactly once, on the cone alone. A change that
#           attributed the write-back to the wrong statement would keep the
#           total at four and still fail here.
#
#           Deliberately not asserted: the split *declaration*'s code info. A
#           later refinement rebuilds signals as variables carrying only name,
#           type and value, so it is discarded before output regardless of what
#           this transformation sets - hif-frontend#37. Asserting it would
#           encode that loss as expected behaviour.
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
    COMMAND ${VERILOG2HIF_EXECUTABLE} -o cone_writeback_code_info ${FIXTURE}
    WORKING_DIRECTORY ${WORK_DIR}
    RESULT_VARIABLE result
    OUTPUT_VARIABLE tool_output
    ERROR_VARIABLE tool_output
)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "verilog2hif failed with exit code ${result}\n${tool_output}")
endif()

set(HIF_FILE ${WORK_DIR}/cone_writeback_code_info.hif.xml)
if(NOT EXISTS ${HIF_FILE})
    message(FATAL_ERROR "Expected HIF file not produced: ${HIF_FILE}")
endif()

file(READ ${HIF_FILE} hif_content)

# The rewrite has to have run at all. If a future change stops splitting
# part-select targets, everything below would pass vacuously rather than fail,
# so say so instead.
string(FIND "${hif_content}" "y_partial" partial_at)
if(partial_at EQUAL -1)
    message(FATAL_ERROR
        "No split signal was generated for the part-select targets, so this test is no longer "
        "exercising the write-back this fix is about (hif-muffin#24). If the rewrite has changed "
        "shape, this test needs rewriting rather than deleting.\nFull content:\n${hif_content}")
endif()

# Two source assignments, each becoming a cone assignment plus a write-back.
string(REGEX MATCHALL "<ASSIGN>" assign_matches "${hif_content}")
list(LENGTH assign_matches assign_count)
if(NOT assign_count EQUAL 4)
    message(FATAL_ERROR
        "Expected 4 assignments (a cone and a write-back for each of the two source statements) "
        "but found ${assign_count}. The rewrite has changed shape, so the per-line counts below "
        "no longer mean what this test assumes.\nFull content:\n${hif_content}")
endif()

# Code info is written as the last child of the object it belongs to, so an
# Assign's own attribution is the CODE_INFO immediately preceding </ASSIGN>.
# A CODE_INFO anywhere else in the element belongs to the target or the source
# expression and is not what was missing.
string(REGEX MATCHALL
    "<CODE_INFO[^>]*/>[ \t\r\n]*</ASSIGN>"
    attributed_matches "${hif_content}")
list(LENGTH attributed_matches attributed_count)
if(NOT attributed_count EQUAL 4)
    message(FATAL_ERROR
        "Only ${attributed_count} of ${assign_count} assignments carry their own CODE_INFO. The "
        "write-back assignment synthesized for a part-select continuous assign is unattributed, so "
        "two fault locations on one signal are indistinguishable (hif-muffin#24).\nFull content:\n"
        "${hif_content}")
endif()

# The two source statements. Each must account for exactly two attributed
# assignments: its cone and its write-back. Before the fix each accounted for
# one. The file attribute is required to be non-empty in the same match, since
# an empty source is half of the reported symptom.
foreach(source_line 36 37)
    string(REGEX MATCHALL
        "<CODE_INFO[^>]*file=\"[^\"]+\"[^>]*line_number=\"${source_line}\"[^>]*/>[ \t\r\n]*</ASSIGN>"
        line_matches "${hif_content}")
    list(LENGTH line_matches line_count)
    if(NOT line_count EQUAL 2)
        message(FATAL_ERROR
            "Line ${source_line} of the fixture is the source of ${line_count} attributed "
            "assignment(s), expected 2 - the cone assignment and its write-back. Before the fix "
            "for hif-muffin#24 it was 1, the cone alone, because the write-back was created "
            "without code info.\nFull content:\n${hif_content}")
    endif()
endforeach()

message(STATUS "cone_writeback_code_info test passed: 4 assignments, all attributed, 2 per source line.")

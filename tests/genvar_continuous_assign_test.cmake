# -----------------------------------------------------------------------------
# @brief  : Regression (hif-frontend#23): verilog2hif aborted on a continuous
#           assignment inside a `for generate` whose right-hand side or target
#           index reads the genvar.
#
#             [HIF] [getDeclaration] - ERROR: Declaration not found
#             - Raised by hif-core/src/semantics/declarationUtils.cpp:1658
#             - Source file info - g_min.v: line 5, column 17
#               -- in SubProgram: hif_cone_y_partial0_0_0
#
#           splitLogicConesLoops replaces a bit-select target with a fresh
#           signal, and generateConeFunctions later copies the assignment into a
#           procedure declared beside that signal. The signal was declared
#           beside its *target* - module scope - so the copy of the assignment
#           left the genvar's scope, and standardization aborted on the
#           reference at "STD 01: Simplifying source tree".
#
#           Exit 0 is not a sufficient check, and the structural assertion below
#           is the point. What makes the reference resolve is *where* the split
#           declaration ends up: inside the ForGenerate, which is the same scope
#           the genvar is declared in. A future change that put it back at
#           module scope but happened to silence the abort some other way would
#           still be wrong - and would also reintroduce the second problem,
#           which produces no diagnostic at all: one signal outside the loop is
#           shared by every iteration, so all of them drive the same net.
#
#           No round-trip leg. The regenerated design cannot be checked, because
#           neither backend handles a ForGenerate: hif2verilog emits invalid
#           Verilog and hif2vhdl silently drops the loop (hif-backend#78). That
#           is pre-existing and independent - the procedural form of this same
#           loop, which has always translated at exit 0, regenerates identically
#           broken before and after this fix. This test therefore stops where
#           this repository's responsibility does.
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
    COMMAND ${VERILOG2HIF_EXECUTABLE} -o genvar_continuous_assign ${FIXTURE}
    WORKING_DIRECTORY ${WORK_DIR}
    RESULT_VARIABLE result
    OUTPUT_VARIABLE tool_output
    ERROR_VARIABLE tool_output
)
if(NOT result EQUAL 0)
    message(FATAL_ERROR
        "verilog2hif failed with exit code ${result} -- which is the reported symptom of "
        "hif-frontend#23.\n${tool_output}")
endif()

set(HIF_FILE ${WORK_DIR}/genvar_continuous_assign.hif.xml)
if(NOT EXISTS ${HIF_FILE})
    message(FATAL_ERROR "Expected HIF file not produced: ${HIF_FILE}")
endif()

file(READ ${HIF_FILE} hif_content)

# The loop has to still be a loop. If a future change unrolls it before this
# point the rest of this test is meaningless rather than failing, so say so.
string(FIND "${hif_content}" "<FORGENERATE" fg_begin)
if(fg_begin EQUAL -1)
    message(FATAL_ERROR
        "The translation contains no ForGenerate, so this test is no longer exercising the scope "
        "question it is about (hif-frontend#23). If unrolling now happens earlier, this test needs "
        "rewriting rather than deleting.")
endif()

string(FIND "${hif_content}" "</FORGENERATE>" fg_end)
if(fg_end EQUAL -1)
    message(FATAL_ERROR "Malformed HIF: a <FORGENERATE> with no closing tag.")
endif()

# The split declaration and its cone must be inside the loop, because that is
# the scope the genvar is declared in. Position comparison rather than a regex:
# the fixture has exactly one ForGenerate, so "between the tags" is exact, and
# CMake has no non-greedy match to carve the element out with.
string(FIND "${hif_content}" "hif_cone_y_partial" cone_at)
if(cone_at EQUAL -1)
    message(FATAL_ERROR
        "No cone procedure was generated for the split bit-select target, so the code path this "
        "test covers did not run (hif-frontend#23).\nFull content:\n${hif_content}")
endif()

if(cone_at LESS fg_begin OR cone_at GREATER fg_end)
    message(FATAL_ERROR
        "The cone procedure for the split target is declared outside the ForGenerate (at offset "
        "${cone_at}, loop spans ${fg_begin}..${fg_end}). Its body copies an assignment that reads "
        "the genvar, so at module scope that reference has no declaration and standardization "
        "aborts - and one signal outside the loop would additionally be shared by every iteration "
        "(hif-frontend#23).")
endif()

string(FIND "${hif_content}" "y_partial" partial_at)
if(partial_at LESS fg_begin OR partial_at GREATER fg_end)
    message(FATAL_ERROR
        "The split signal for the bit-select target is declared outside the ForGenerate (at offset "
        "${partial_at}, loop spans ${fg_begin}..${fg_end}), so every iteration of the unrolled loop "
        "would drive the same net (hif-frontend#23).")
endif()

message(STATUS "genvar_continuous_assign test passed.")

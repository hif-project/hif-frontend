# -----------------------------------------------------------------------------
# @brief  : Regression (hif-frontend#33): verilog2hif refused SystemVerilog
#           `logic` in an ANSI-style port header, though #21 had made it work
#           as a body declaration.
#
#             -- ERROR: discipline_and_modifiers: IDENTIFIER is not supported
#             verilog2hif: verilog_support.cpp:219: yyerror: Assertion `false' failed.
#
#           `logic` is not a lexer keyword, so a port header naming it reached
#           the Verilog-AMS discipline rule as a bare identifier and was
#           rejected at parse time - before any refinement could decide what the
#           name meant.
#
#           Exit 0 alone is not a sufficient check. A fix that made the header
#           parse but gave the port a different type, signedness or default
#           value would also exit 0 while silently changing the design. Each
#           fixture is therefore paired with a twin that is already supported
#           and means the same thing, and the two translations are required to
#           match exactly, modulo source position.
#
#           The two pairs use different twins on purpose:
#             - the input/output pair is twinned against Verilog-2001, where
#               `logic`/`reg` and `input logic`/`input` are exactly equivalent;
#             - the inout pair is twinned against the *body* `logic` spelling,
#               because `inout logic` is NOT a plain `inout` - a variable reads
#               'X' uninitialised where a net reads 'Z' (#21) - so the net form
#               would be the wrong reference.
# @author : Enrico Fraccaroli
# -----------------------------------------------------------------------------

foreach(required VERILOG2HIF_EXECUTABLE FIXTURE_DIR WORK_DIR)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "Missing required variable: ${required}")
    endif()
endforeach()

file(REMOVE_RECURSE ${WORK_DIR})
file(MAKE_DIRECTORY ${WORK_DIR})

function(translate label fixture out_content)
    execute_process(
        COMMAND ${VERILOG2HIF_EXECUTABLE} -o ${label} ${fixture}
        WORKING_DIRECTORY ${WORK_DIR}
        RESULT_VARIABLE result
        OUTPUT_VARIABLE tool_output
        ERROR_VARIABLE tool_output
    )
    if(NOT result EQUAL 0)
        message(FATAL_ERROR
            "verilog2hif failed on ${fixture} with exit code ${result}.\n${tool_output}")
    endif()

    set(produced ${WORK_DIR}/${label}.hif.xml)
    if(NOT EXISTS ${produced})
        message(FATAL_ERROR "Expected HIF file not produced: ${produced}")
    endif()

    file(READ ${produced} content)
    # Source positions legitimately differ between the two fixtures; nothing
    # else may.
    string(REGEX REPLACE "[ \t]*<CODE_INFO[^>]*/>" "" content "${content}")
    string(REGEX REPLACE "[ \t](file|filename|number|generationDate)=\"[^\"]*\"" "" content "${content}")
    set(${out_content} "${content}" PARENT_SCOPE)
endfunction()

function(require_same what a_label a_content b_label b_content)
    if(NOT a_content STREQUAL b_content)
        file(WRITE ${WORK_DIR}/${a_label}_normalised.xml "${a_content}")
        file(WRITE ${WORK_DIR}/${b_label}_normalised.xml "${b_content}")
        message(FATAL_ERROR
            "${what}\n"
            "Normalised outputs written to ${WORK_DIR}/${a_label}_normalised.xml and "
            "${WORK_DIR}/${b_label}_normalised.xml for diffing.")
    endif()
endfunction()

# 1. The refusal itself: the ANSI `logic` header must translate at all.
translate(sv_logic_ansi_port ${FIXTURE_DIR}/sv_logic_ansi_port.v ansi_content)

# 2. Its Verilog-2001 twin, as the reference for what the header has to mean.
translate(sv_logic_ansi_port_reg_twin ${FIXTURE_DIR}/sv_logic_ansi_port_reg_twin.v twin_content)

require_same(
    "An ANSI `logic` port header and its Verilog-2001 twin did not produce the same HIF. They spell the same ports, so the translations must agree on type, signedness and default value - not merely both exit 0."
    sv_logic_ansi_port "${ansi_content}"
    sv_logic_ansi_port_reg_twin "${twin_content}")

# 3. The inout direction, against the body `logic` spelling #21 already
#    supports. This is the fix's exact claim: a port header naming `logic`
#    means what a body declaration naming `logic` means.
translate(sv_logic_ansi_inout ${FIXTURE_DIR}/sv_logic_ansi_inout.v inout_ansi_content)
translate(sv_logic_ansi_inout_body_twin ${FIXTURE_DIR}/sv_logic_ansi_inout_body_twin.v inout_body_content)

require_same(
    "An `inout logic` port header and its body-declared twin did not produce the same HIF. `logic` is a variable in both spellings, so both must carry the 'X' default a variable has - not the 'Z' a net has."
    sv_logic_ansi_inout "${inout_ansi_content}"
    sv_logic_ansi_inout_body_twin "${inout_body_content}")

message(STATUS "sv_logic_ansi_port test passed.")

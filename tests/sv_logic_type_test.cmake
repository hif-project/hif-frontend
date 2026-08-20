# -----------------------------------------------------------------------------
# @brief  : Regression: verilog2hif aborted on the SystemVerilog `logic` type.
#
#           `logic` is not a lexer keyword, so it reaches the post-parsing
#           refinements as an unresolved TypeReference and is handled by
#           FixDescription_1::_fixAMSDisciplines, which reads it as the
#           Verilog-AMS `logic` discipline and renames it `ams_logic`. That
#           typedef lives in the vams_disciplines standard library, which a
#           plain Verilog input never loads, so the rename left a TypeReference
#           with no declaration behind and the first pass needing its base type
#           aborted the tool:
#
#             [HIF] [getAllReferences] ASSERT: Declaration not found.
#             - Raised by hif-core/src/semantics/referencesUtils.cpp:168
#
#           (scalars), or getBaseType.cpp:31 "Not found declaration of TypeRef"
#           for a vector.
#
#           Exit 0 alone is not a sufficient check. `logic` and `reg` spell the
#           same four-state variable in SystemVerilog, and they must agree on
#           more than parsing: a fix that resolved the type but left the
#           declaration with *net* semantics would also exit 0 while silently
#           giving the signal a 'Z' initial value where `reg` gives 'X'. The
#           two fixtures are therefore identical but for the keyword, and their
#           translations are required to match exactly, modulo source position.
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

# 1. The crash itself: the `logic` fixture must translate at all.
translate(sv_logic_type ${FIXTURE_DIR}/sv_logic_type.v logic_content)

# 2. The `reg` twin, as the reference for what `logic` has to mean.
translate(sv_logic_type_reg_twin ${FIXTURE_DIR}/sv_logic_type_reg_twin.v reg_content)

if(NOT logic_content STREQUAL reg_content)
    file(WRITE ${WORK_DIR}/logic_normalised.xml "${logic_content}")
    file(WRITE ${WORK_DIR}/reg_normalised.xml "${reg_content}")
    message(FATAL_ERROR
        "`logic` and `reg` did not produce the same HIF.\n"
        "They spell the same four-state variable, so the translations must agree "
        "on type and on uninitialised value ('X', not 'Z').\n"
        "Normalised outputs written to ${WORK_DIR}/logic_normalised.xml and "
        "${WORK_DIR}/reg_normalised.xml for diffing.")
endif()

# 3. Guard the Verilog-AMS reading: there `logic` *is* a discipline and must
#    still resolve to the ams_logic typedef, i.e. the fallback must not fire.
translate(sv_logic_ams_discipline ${FIXTURE_DIR}/sv_logic_ams_discipline.vams ams_content)

if(NOT ams_content MATCHES "ams_logic")
    message(FATAL_ERROR
        "The Verilog-AMS `logic` discipline no longer maps to the ams_logic typedef - "
        "the SystemVerilog fallback fired where the disciplines library was available.")
endif()

message(STATUS "sv_logic_type test passed.")

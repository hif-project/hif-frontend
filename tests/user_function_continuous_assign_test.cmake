# -----------------------------------------------------------------------------
# @brief  : Regression (hif-frontend#14): verilog2hif aborted with hif-core's
#           "Declaration not found" on a user-defined Verilog function, so the
#           design could not be translated at all.
#
#           The trigger is narrower than the issue's title: the call has to be
#           in a continuous assignment whose target is an *output port*. A call
#           inside an always block, or a continuous assignment to an internal
#           wire, always worked.
#
#           Cause: FixDescription_1::visitGlobalAction folded an assignment
#           with a constant-looking right-hand side into the target's
#           declaration. For an output port that declaration is on the Entity,
#           which cannot see Contents declarations, so the call was stranded
#           with nothing to resolve to.
#
#           Exit 0 is therefore not a sufficient check: a fold that quietly
#           dropped the assignment would also exit 0. This test requires the
#           translation to be *right* - the function present, the call still
#           in the global action, and nothing referring to a Contents
#           declaration left inside the Entity.
#
#           The second fixture covers a plain `localparam`, which failed
#           identically and shows the defect was never about functions, and
#           carries contrast cases (module parameter, system function, literal)
#           that were always reachable from the Entity and must keep folding.
# @author : Enrico Fraccaroli
# -----------------------------------------------------------------------------

foreach(required VERILOG2HIF_EXECUTABLE FUNCTION_FIXTURE LOCALPARAM_FIXTURE WORK_DIR)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "Missing required variable: ${required}")
    endif()
endforeach()

file(REMOVE_RECURSE ${WORK_DIR})
file(MAKE_DIRECTORY ${WORK_DIR})

# Returns the text between the first <ENTITY and its matching </ENTITY>.
# The Entity is where a folded value ends up, so it is the region that has to
# be free of references to Contents declarations.
function(entity_section content out_section)
    string(FIND "${content}" "<ENTITY" start)
    string(FIND "${content}" "</ENTITY>" stop)
    if(start EQUAL -1 OR stop EQUAL -1)
        message(FATAL_ERROR "Could not locate the ENTITY section in the produced HIF.")
    endif()
    math(EXPR length "${stop} - ${start}")
    string(SUBSTRING "${content}" ${start} ${length} section)
    set(${out_section} "${section}" PARENT_SCOPE)
endfunction()

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
            "verilog2hif failed on ${fixture} with exit code ${result} (hif-frontend#14).\n${tool_output}")
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

# --------------------------------------------------------------------------
# Fixture 1: the user-defined function.
# --------------------------------------------------------------------------
translate(user_function ${FUNCTION_FIXTURE} function_hif)

# The function itself has to survive, with its parameter.
if(NOT function_hif MATCHES "<FUNCTION[^>]*name=\"bump\"")
    message(FATAL_ERROR
        "The user-defined function 'bump' is missing from the produced HIF.\nContent:\n${function_hif}")
endif()
if(NOT function_hif MATCHES "<PARAMETER[^>]*name=\"v\"")
    message(FATAL_ERROR
        "The function's parameter 'v' is missing from the produced HIF; the call could not bind to it.\n"
        "Content:\n${function_hif}")
endif()

# Both calls have to survive.
string(REGEX MATCHALL "<FCALL[^>]*name=\"bump\"" calls "${function_hif}")
list(LENGTH calls call_count)
if(NOT call_count EQUAL 2)
    message(FATAL_ERROR
        "Expected 2 calls to 'bump' in the produced HIF, found ${call_count}. An assignment was dropped "
        "rather than translated (hif-frontend#14).\nContent:\n${function_hif}")
endif()

# And they must still be in the global action, not folded onto the Entity.
# This is the fix stated directly: a Contents declaration must not be
# referenced from inside the Entity, because nothing there can resolve it.
entity_section("${function_hif}" function_entity)
if(function_entity MATCHES "bump")
    message(FATAL_ERROR
        "The call to 'bump' was folded into a port declaration on the Entity, where the function - a "
        "Contents declaration - cannot be resolved. This is the defect (hif-frontend#14).\n"
        "ENTITY section:\n${function_entity}")
endif()
# The assignments themselves have to survive as drivers. By the end of the
# frontend a continuous assignment to an output port has deliberately become a
# process (fixOutputPorts), so what is checked here is that two of them exist -
# not that a GlobalAction does.
string(REGEX MATCHALL "<STATETABLE[^>]*name=\"globact_process" processes "${function_hif}")
list(LENGTH processes process_count)
if(NOT process_count EQUAL 2)
    message(FATAL_ERROR
        "Expected the two continuous assignments to survive as two processes, found ${process_count}. "
        "An assignment was lost rather than translated (hif-frontend#14).\nContent:\n${function_hif}")
endif()

# --------------------------------------------------------------------------
# Fixture 2: the localparam, plus the cases that must keep folding.
# --------------------------------------------------------------------------
translate(localparam_ca ${LOCALPARAM_FIXTURE} localparam_hif)

if(NOT localparam_hif MATCHES "name=\"K\"")
    message(FATAL_ERROR
        "The localparam 'K' is missing from the produced HIF.\nContent:\n${localparam_hif}")
endif()

entity_section("${localparam_hif}" localparam_entity)
if(localparam_entity MATCHES "\"K\"")
    message(FATAL_ERROR
        "The localparam 'K' was folded into a port declaration on the Entity, where a Contents "
        "declaration cannot be resolved. Same defect as the function case (hif-frontend#14).\n"
        "ENTITY section:\n${localparam_entity}")
endif()

# The contrast cases. These were always reachable from the Entity, so they must
# still be folded - the fix narrows what is foldable, it does not stop folding.
# If these stopped folding, the fix would be over-broad and this would say so.
if(NOT localparam_entity MATCHES "name=\"s\"")
    message(FATAL_ERROR "Expected output port 's' on the Entity.\nENTITY section:\n${localparam_entity}")
endif()
if(NOT localparam_entity MATCHES "<VALUE>")
    message(FATAL_ERROR
        "No port on the Entity carries a folded value any more. The fix is over-broad: a literal, a module "
        "parameter and a system function are all resolvable from the Entity and must keep folding.\n"
        "ENTITY section:\n${localparam_entity}")
endif()

message(STATUS "user_function_continuous_assign test passed.")

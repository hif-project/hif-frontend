# -----------------------------------------------------------------------------
# @brief  : Regression (hif-backend#88, #89, #104): continuous assigns that carry
#           net semantics were lowered to processes, and the net became a reg.
#
#           Lowering a continuous assign loses what makes its target a net:
#             #88  two drivers are resolved by the simulator; two processes
#                  writing one reg are not, the last one wins;
#             #89  a reader sees the net; after lowering it saw a copy of the
#                  driver's expression, blind to anyone else driving the pin;
#             #104 a bit nothing drives is z; a reg starts x.
#
#           They are now kept as continuous assigns when two of them provably
#           write the same bit, when the net is an output/inout port that is read,
#           or when they provably leave a bit undriven. The controls are the
#           shapes that must still be lowered: one whole driver nobody reads, and
#           disjoint per-bit drivers that cover the net.
#
#           What is counted is the HIF: assigns left in the GlobalAction, and
#           processes (StateTables) created from them.
# -----------------------------------------------------------------------------

foreach(required VERILOG2HIF_EXECUTABLE FIXTURE WORK_DIR)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "Missing required variable: ${required}")
    endif()
endforeach()

file(REMOVE_RECURSE ${WORK_DIR})
file(MAKE_DIRECTORY ${WORK_DIR})

execute_process(
    COMMAND ${VERILOG2HIF_EXECUTABLE} -o keep_net_semantics ${FIXTURE}
    WORKING_DIRECTORY ${WORK_DIR}
    RESULT_VARIABLE result
    OUTPUT_VARIABLE tool_output
    ERROR_VARIABLE tool_output
)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "verilog2hif failed with exit code ${result}.\n${tool_output}")
endif()

set(HIF_FILE ${WORK_DIR}/keep_net_semantics.hif.xml)
if(NOT EXISTS ${HIF_FILE})
    message(FATAL_ERROR "Expected HIF file not produced: ${HIF_FILE}")
endif()
file(READ ${HIF_FILE} hif_content)

# The text of one design unit.
function(design_unit name out)
    string(REGEX MATCH "<DESIGNUNIT [^>]*name=\"${name}\".*</DESIGNUNIT>" tail "${hif_content}")
    if(tail STREQUAL "")
        message(FATAL_ERROR "Design unit '${name}' not found in the HIF.")
    endif()
    # Up to the end of this unit: the next design unit starts a new one.
    string(SUBSTRING "${tail}" 12 -1 rest)
    string(FIND "${rest}" "<DESIGNUNIT " next_start)
    if(next_start GREATER -1)
        math(EXPR cut "${next_start} + 12")
        string(SUBSTRING "${tail}" 0 ${cut} tail)
    endif()
    set(${out} "${tail}" PARENT_SCOPE)
endfunction()

# Assigns left in the GlobalAction, and processes.
function(count_in unit_name assigns_var processes_var)
    design_unit(${unit_name} unit)
    string(REGEX MATCH "<GLOBALACTION>.*</GLOBALACTION>" globact "${unit}")
    string(REGEX MATCHALL "<ASSIGN>" assigns "${globact}")
    list(LENGTH assigns assign_count)
    string(REGEX MATCHALL "<STATETABLE " processes "${unit}")
    list(LENGTH processes process_count)
    set(${assigns_var} ${assign_count} PARENT_SCOPE)
    set(${processes_var} ${process_count} PARENT_SCOPE)
endfunction()

function(expect_kept unit_name expected_assigns expected_processes why)
    count_in(${unit_name} assigns processes)
    if(NOT assigns EQUAL ${expected_assigns} OR NOT processes EQUAL ${expected_processes})
        message(FATAL_ERROR
            "${unit_name}: expected ${expected_assigns} continuous assigns kept and ${expected_processes} "
            "processes, found ${assigns} assigns and ${processes} processes. ${why}")
    endif()
endfunction()

expect_kept(two_drivers 2 0 "Two drivers on one net were lowered to processes writing one reg (hif-backend#88).")
# Each part-select target is split into a helper net, driven by one assign and read
# only by the assign that rebuilds the target, so two partial drivers are four assigns:
# the helpers are kept with the net, or the value they carry would be dropped.
expect_kept(partial_port 4 0 "A port with undriven bits was lowered, so those bits read x, not z (hif-backend#104).")
# w and its helper nets are kept (four assigns); y = w is one whole driver nobody reads,
# so it is still lowered to its one process.
expect_kept(partial_wire 4 1 "A net with undriven bits was lowered, so those bits read x, not z (hif-backend#104).")

# The same shapes with an index that is not an integer literal: a for generate leaves a bit
# string once expanded, and a parameter is a name (hif-backend#104, #88). Each target is split
# into a helper net and a rebuilding assign, so three bit drivers are six assigns and five are ten.
expect_kept(partial_generate 6 0 "A for generate that leaves a bit undriven was lowered, so that bit reads x, not z (hif-backend#104).")
expect_kept(parameter_index 10 0 "Two drivers of one bit, one indexed by a parameter, were lowered and no longer resolved (hif-backend#88).")

# read_back: the inout's driver is kept, and the reader reads the port. The
# reader is a process, so one is expected; what must not be there is the
# `_out_sig` copy of the driver's expression.
count_in(read_back read_back_assigns read_back_processes)
if(NOT read_back_assigns EQUAL 1)
    message(FATAL_ERROR
        "read_back: expected the inout's driver kept as 1 continuous assign, found ${read_back_assigns} "
        "(hif-backend#89).")
endif()
design_unit(read_back read_back_unit)
if(read_back_unit MATCHES "_out_sig")
    message(FATAL_ERROR
        "read_back: the reader was given a copy of the driver's expression (bus_out_sig) instead of "
        "reading the port (hif-backend#89).")
endif()

# Controls: these shapes must still be lowered.
foreach(control single_driver per_bit)
    count_in(${control} control_assigns control_processes)
    if(NOT control_assigns EQUAL 0 OR control_processes EQUAL 0)
        message(FATAL_ERROR
            "${control}: expected the continuous assigns lowered to processes, found ${control_assigns} "
            "assigns left and ${control_processes} processes. The rule kept something it should not.")
    endif()
endforeach()

message(STATUS "keep_net_semantics test passed.")

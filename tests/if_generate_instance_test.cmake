# -----------------------------------------------------------------------------
# @brief  : Regression (hif-frontend#32): verilog2hif aborted on an `if generate`
#           with an else branch, in a module that also contains an instance.
#
#             [HIF] [Simplify] - ASSERT: Cannot resolve if generate condition
#             - Raised by hif-core/src/manipulation/simplify.cpp:4399
#
#           The condition was not the problem, despite the message: a literal
#           `if (1)` failed identically to the parameterised reproducer. Verilog
#           lowers `if (<cond>)` to `or_reduce(<cond>)` and builds the else
#           condition as its negation, and hif-core's constant folder had a
#           `// TODO` where a reduction over a constant should be folded, so the
#           else condition stayed an Expression where _simplifyIfGenerate needs
#           a ConstValue.
#
#           The instance makes the failure reachable rather than causing it:
#           only because the module contains one does partialFlattening run
#           flattenDesign, which is what requests generate expansion.
#
#           Exit 0 alone is not enough here - it would also hold if the wrong
#           branch were selected, or both. USE_LEAF defaults to 1, so the leaf's
#           `&` must survive and the else branch's `|` must not.
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
    COMMAND ${VERILOG2HIF_EXECUTABLE} -o if_generate_instance ${FIXTURE}
    WORKING_DIRECTORY ${WORK_DIR}
    RESULT_VARIABLE result
    OUTPUT_VARIABLE tool_output
    ERROR_VARIABLE tool_output
)
if(NOT result EQUAL 0)
    message(FATAL_ERROR
        "verilog2hif failed with exit code ${result} -- which is the reported symptom of "
        "hif-frontend#32.\n${tool_output}")
endif()

set(HIF_FILE ${WORK_DIR}/if_generate_instance.hif.xml)
if(NOT EXISTS ${HIF_FILE})
    message(FATAL_ERROR "Expected HIF file not produced: ${HIF_FILE}")
endif()

file(READ ${HIF_FILE} hif_content)

# The generate must be gone: the condition is static, so exactly one branch
# survives it.
if(hif_content MATCHES "<IFGENERATE")
    message(FATAL_ERROR
        "An IfGenerate survives in the translated design, so its condition was never resolved "
        "(hif-frontend#32).\nFull content:\n${hif_content}")
endif()

# USE_LEAF defaults to 1, so the surviving branch is the instance, flattened in.
if(NOT hif_content MATCHES "operator=\"&amp;\"")
    message(FATAL_ERROR
        "The leaf's `&` is absent, so the `USE_LEAF = 1` branch did not survive. Selecting the "
        "wrong branch of a static if generate is silent - the tool still exits 0 "
        "(hif-frontend#32).\nFull content:\n${hif_content}")
endif()

if(hif_content MATCHES "operator=\"\\|\"")
    message(FATAL_ERROR
        "The else branch's `|` is present. With `USE_LEAF = 1` that branch is not taken, so "
        "either the wrong branch survived or both did (hif-frontend#32).\n"
        "Full content:\n${hif_content}")
endif()

message(STATUS "if_generate_instance test passed.")

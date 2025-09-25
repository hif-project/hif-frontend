# -----------------------------------------------------------------------------
# @brief  : HIF cmake file.
# @author : Enrico Fraccaroli
# -----------------------------------------------------------------------------

# Set the minimum CMake version, the project name and default build type.
cmake_minimum_required(VERSION 3.1...3.18)

# Set the project name.
project(frontend LANGUAGES CXX)

# Set the default build type to Debug.
if(NOT CMAKE_BUILD_TYPE)
    message(STATUS "Setting build type to 'Debug' as none was specified.")
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Choose the type of build." FORCE)
endif()

# Set the install prefix.
set(CMAKE_INSTALL_PREFIX /usr/local)

# Set modules path.
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${PROJECT_SOURCE_DIR}/cmake)

# -----------------------------------------------------------------------------
# OPTIONS
# -----------------------------------------------------------------------------

option(STRICT_WARNINGS "Enable strict compiler warnings" ON)
option(WARNINGS_AS_ERRORS "Treat all warnings as errors" OFF)

# -----------------------------------------------------------------------------
# ENABLE FETCH CONTENT
# -----------------------------------------------------------------------------

# We need this in order to import external projects.
include(FetchContent)
# Hide fetchcontent variables.
mark_as_advanced(FORCE
    FETCHCONTENT_QUIET
    FETCHCONTENT_BASE_DIR
    FETCHCONTENT_FULLY_DISCONNECTED
    FETCHCONTENT_UPDATES_DISCONNECTED
)

# -----------------------------------------------------------------------------
# DEPENDENCY (SYSTEM LIBRARIES)
# -----------------------------------------------------------------------------

find_package(Doxygen)
find_package(HIF REQUIRED)
find_package(FLEX REQUIRED)
find_package(BISON REQUIRED)

find_program(CLANG_TIDY_EXE NAMES clang-tidy)

# -----------------------------------------------------------------------------
# -----------------------------------------------------------------------------
# VERILOG2HIF
# -----------------------------------------------------------------------------
# -----------------------------------------------------------------------------

# =====================================
# PARSER
# =====================================

bison_target(
    verilog_parser
    "${PROJECT_SOURCE_DIR}/grammar/verilog.yxx"
    "${PROJECT_BINARY_DIR}/verilog_parser.cpp"
)

flex_target(
    verilog_lexer
    "${PROJECT_SOURCE_DIR}/grammar/verilog.lex"
    "${PROJECT_BINARY_DIR}/verilog_lexer.cpp"
)

ADD_FLEX_BISON_DEPENDENCY(verilog_lexer verilog_parser)

# =====================================
# TARGET
# =====================================

add_executable(
    verilog2hif
    ${PROJECT_SOURCE_DIR}/src/verilog2hif/verilog2hif.cpp
    ${PROJECT_SOURCE_DIR}/src/verilog2hif/verilog2hif_parse_line.cpp
    ${PROJECT_SOURCE_DIR}/src/verilog2hif/verilog_support.cpp
    ${PROJECT_SOURCE_DIR}/src/verilog2hif/verilog_parser.cpp
    ${PROJECT_SOURCE_DIR}/src/verilog2hif/verilog_parser_extension.cpp
    ${PROJECT_SOURCE_DIR}/src/verilog2hif/FixDescription_1.cpp
    ${PROJECT_SOURCE_DIR}/src/verilog2hif/FixDescription_2.cpp
    ${PROJECT_SOURCE_DIR}/src/verilog2hif/FixDescription_3.cpp
    ${PROJECT_SOURCE_DIR}/src/verilog2hif/mark_ams_language.cpp
    ${PROJECT_SOURCE_DIR}/src/verilog2hif/verilog_parser_struct.cpp
    ${BISON_verilog_parser_OUTPUTS}
    ${FLEX_verilog_lexer_OUTPUTS}
)

target_include_directories(
    verilog2hif PUBLIC
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_BINARY_DIR}
    ${HIF_INCLUDE_DIR}
)

target_link_libraries(
    verilog2hif PUBLIC
    ${HIF_LIBRARY}
)

# =====================================
# COMPILATION FLAGS
# =====================================

if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    # Disable warnings for MSVC-specific "safe" functions like strcpy_s, etc.,
    # which are not portable and may clutter warning logs.
    target_compile_definitions(verilog2hif PUBLIC _CRT_SECURE_NO_WARNINGS)

    # Disable warning C4702: unreachable code.
    add_compile_options(/wd4702)

    if(WARNINGS_AS_ERRORS)
        # Treat all warnings as errors to enforce stricter code quality.
        target_compile_options(verilog2hif PUBLIC /WX)
    endif()

    if(STRICT_WARNINGS)
        # Enable external header management to suppress warnings in system and
        # external headers, making it easier to focus on project-specific issues.
        target_compile_options(verilog2hif PUBLIC /experimental:external)
        target_compile_options(verilog2hif PUBLIC /external:I ${CMAKE_BINARY_DIR})
        target_compile_options(verilog2hif PUBLIC /external:anglebrackets)
        target_compile_options(verilog2hif PUBLIC /external:W0)

        # Use a high warning level to catch as many potential issues as possible.
        target_compile_options(verilog2hif PUBLIC /W4)

        # Enforce standards-compliant behavior to avoid relying on MSVC-specific extensions.
        target_compile_options(verilog2hif PUBLIC /permissive-)
    endif()

elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    if(WARNINGS_AS_ERRORS)
        # Treat all warnings as errors to enforce stricter code quality.
        target_compile_options(verilog2hif PUBLIC -Werror)
    endif()

    if(STRICT_WARNINGS)
        # Enable a broad set of warnings to catch common and subtle issues:
        target_compile_options(verilog2hif PUBLIC
            -Wall                # Enable most general-purpose warnings.
            -Wextra              # Enable extra warnings not included in -Wall.
            -Wconversion         # Warn about implicit type conversions that may lose data.
            -pedantic            # Enforce strict compliance with the C++ standard.
            -Wshadow             # Warn about variable shadowing, which can cause subtle bugs.
            -Wnon-virtual-dtor   # Warn when a class with virtual functions lacks a virtual destructor.
            -Wnull-dereference   # Warn about potential null pointer dereferences.
            -Wformat=2           # Enable strict checks for printf/scanf format strings.
            -Woverloaded-virtual # Warn when a derived class function hides a base class virtual function.
            -Wfloat-equal        # Warn about direct comparisons of floating-point values, which can be imprecise.
        )
    endif()
endif()

# =====================================
# INSTALL
# =====================================

install(TARGETS verilog2hif DESTINATION bin)

# -----------------------------------------------------------------------------
# -----------------------------------------------------------------------------
# VHDL2HIF
# -----------------------------------------------------------------------------
# -----------------------------------------------------------------------------

bison_target(
    vhdl_parser
    "${PROJECT_SOURCE_DIR}/grammar/vhdl.yxx"
    "${PROJECT_BINARY_DIR}/vhdlParser.cpp"
)

flex_target(
    vhdl_lexer
    "${PROJECT_SOURCE_DIR}/grammar/vhdl.lex"
    "${PROJECT_BINARY_DIR}/vhdlLexer.cpp"
)
ADD_FLEX_BISON_DEPENDENCY(vhdl_lexer vhdl_parser)

add_executable(
    vhdl2hif
    ${PROJECT_SOURCE_DIR}/src/vhdl2hif/vhdl2hif.cpp
    ${PROJECT_SOURCE_DIR}/src/vhdl2hif/vhdl2hifParseLine.cpp
    ${PROJECT_SOURCE_DIR}/src/vhdl2hif/vhdl_support.cpp
    ${PROJECT_SOURCE_DIR}/src/vhdl2hif/VhdlParser.cpp
    ${PROJECT_SOURCE_DIR}/src/vhdl2hif/VhdlParser_psl.cpp
    ${PROJECT_SOURCE_DIR}/src/vhdl2hif/VhdlParser_extension.cpp
    ${PROJECT_SOURCE_DIR}/src/vhdl2hif/PostParsingVisitor_fixRanges.cpp
    ${PROJECT_SOURCE_DIR}/src/vhdl2hif/PostParsingVisitor_step1.cpp
    ${PROJECT_SOURCE_DIR}/src/vhdl2hif/PostParsingVisitor_step2.cpp
    ${BISON_vhdl_parser_OUTPUTS}
    ${FLEX_vhdl_lexer_OUTPUTS}
)
# Add includes.
target_include_directories(
    vhdl2hif PUBLIC
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_BINARY_DIR}
    ${HIF_INCLUDE_DIR}
)
# Link libraries.
target_link_libraries(
    vhdl2hif PUBLIC
    ${HIF_LIBRARY}
)

# =====================================
# COMPILATION FLAGS
# =====================================

if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    # Disable warnings for MSVC-specific "safe" functions like strcpy_s, etc.,
    # which are not portable and may clutter warning logs.
    target_compile_definitions(vhdl2hif PUBLIC _CRT_SECURE_NO_WARNINGS)

    # Disable warning C4702: unreachable code.
    add_compile_options(/wd4702)

    if(WARNINGS_AS_ERRORS)
        # Treat all warnings as errors to enforce stricter code quality.
        target_compile_options(vhdl2hif PUBLIC /WX)
    endif()

    if(STRICT_WARNINGS)
        # Enable external header management to suppress warnings in system and
        # external headers, making it easier to focus on project-specific issues.
        target_compile_options(vhdl2hif PUBLIC /experimental:external)
        target_compile_options(vhdl2hif PUBLIC /external:I ${CMAKE_BINARY_DIR})
        target_compile_options(vhdl2hif PUBLIC /external:anglebrackets)
        target_compile_options(vhdl2hif PUBLIC /external:W0)

        # Use a high warning level to catch as many potential issues as possible.
        target_compile_options(vhdl2hif PUBLIC /W4)

        # Enforce standards-compliant behavior to avoid relying on MSVC-specific extensions.
        target_compile_options(vhdl2hif PUBLIC /permissive-)
    endif()

elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    if(WARNINGS_AS_ERRORS)
        # Treat all warnings as errors to enforce stricter code quality.
        target_compile_options(vhdl2hif PUBLIC -Werror)
    endif()

    if(STRICT_WARNINGS)
        # Enable a broad set of warnings to catch common and subtle issues:
        target_compile_options(vhdl2hif PUBLIC
            -Wall                # Enable most general-purpose warnings.
            -Wextra              # Enable extra warnings not included in -Wall.
            -Wconversion         # Warn about implicit type conversions that may lose data.
            -pedantic            # Enforce strict compliance with the C++ standard.
            -Wshadow             # Warn about variable shadowing, which can cause subtle bugs.
            -Wnon-virtual-dtor   # Warn when a class with virtual functions lacks a virtual destructor.
            -Wnull-dereference   # Warn about potential null pointer dereferences.
            -Wformat=2           # Enable strict checks for printf/scanf format strings.
            -Woverloaded-virtual # Warn when a derived class function hides a base class virtual function.
            -Wfloat-equal        # Warn about direct comparisons of floating-point values, which can be imprecise.
        )
    endif()
endif()

# =====================================
# INSTALL
# =====================================

install(TARGETS vhdl2hif DESTINATION bin)

# -----------------------------------------------------------------------------
# CODE ANALYSIS
# -----------------------------------------------------------------------------

if(CLANG_TIDY_EXE)
    file(GLOB_RECURSE ALL_PROJECT_FILES
        "${PROJECT_SOURCE_DIR}/include/**/*.hpp"
        "${PROJECT_SOURCE_DIR}/include/**/*.hh"
        "${PROJECT_SOURCE_DIR}/include/**/*.h"
        "${PROJECT_SOURCE_DIR}/include/*.hpp"
        "${PROJECT_SOURCE_DIR}/include/*.hh"
        "${PROJECT_SOURCE_DIR}/include/*.h"
        "${PROJECT_SOURCE_DIR}/src/**/*.cpp"
        "${PROJECT_SOURCE_DIR}/src/**/*.cc"
        "${PROJECT_SOURCE_DIR}/src/**/*.c"
        "${PROJECT_SOURCE_DIR}/src/*.cpp"
        "${PROJECT_SOURCE_DIR}/src/*.cc"
        "${PROJECT_SOURCE_DIR}/src/*.c"
    )
    add_custom_target(
        ${PROJECT_NAME}_clang_tidy
        COMMAND ${CLANG_TIDY_EXE}
        --p=${CMAKE_BINARY_DIR}
        ${ALL_PROJECT_FILES}
        COMMENT "Running clang-tidy"
        VERBATIM
    )
    add_custom_target(
        ${PROJECT_NAME}_clang_tidy_fix
        COMMAND ${CLANG_TIDY_EXE}
        --fix --fix-errors
        --p=${CMAKE_BINARY_DIR}
        ${ALL_PROJECT_FILES}
        COMMENT "Running clang-tidy-fix"
        VERBATIM
    )
endif()

# -----------------------------------------------------------------------------
# DOCUMENTATION
# -----------------------------------------------------------------------------

if(DOXYGEN_FOUND)
    
    # FetchContent: Doxygen Awesome CSS
    FetchContent_Declare(doxygenawesome
        GIT_REPOSITORY https://github.com/jothepro/doxygen-awesome-css
        GIT_TAG main
    )
    FetchContent_MakeAvailable(doxygenawesome)

    # Hide FetchContent variables to avoid clutter in ccmake.
    mark_as_advanced(FORCE
        FETCHCONTENT_UPDATES_DISCONNECTED_DOXYGENAWESOME
        FETCHCONTENT_SOURCE_DIR_DOXYGENAWESOME
    )

    # Customization: Doxygen Configuration
    set(DOXYGEN_WARN_FORMAT "$file:$line:1: $text")
    set(DOXYGEN_PROJECT_NAME "HIF Frontends")
    set(DOXYGEN_PROJECT_BRIEF "The HIF frontends.")
    set(DOXYGEN_USE_MDFILE_AS_MAINPAGE ${PROJECT_SOURCE_DIR}/README.md)
    set(DOXYGEN_SHOW_INCLUDE_FILES NO)
    set(DOXYGEN_GENERATE_TREEVIEW YES)
    set(DOXYGEN_GENERATE_LATEX NO)
    set(DOXYGEN_GENERATE_MAN NO)

    # Styling and UX enhancements using Doxygen Awesome
    set(DOXYGEN_HTML_HEADER ${doxygenawesome_SOURCE_DIR}/doxygen-custom/header.html)
    set(DOXYGEN_HTML_EXTRA_STYLESHEET ${doxygenawesome_SOURCE_DIR}/doxygen-awesome.css)
    set(DOXYGEN_HTML_EXTRA_FILES
        ${doxygenawesome_SOURCE_DIR}/doxygen-awesome-fragment-copy-button.js
        ${doxygenawesome_SOURCE_DIR}/doxygen-awesome-paragraph-link.js
        ${doxygenawesome_SOURCE_DIR}/doxygen-awesome-darkmode-toggle.js
    )

    # Set stricter warnings for better documentation quality
    set(DOXYGEN_WARN_IF_UNDOCUMENTED YES)
    set(DOXYGEN_WARN_IF_DOC_ERROR YES)
    set(DOXYGEN_WARN_NO_PARAMDOC YES)
    set(DOXYGEN_WARN_AS_ERROR NO)

    # Add Doxygen documentation target.
    file(GLOB_RECURSE ALL_PROJECT_FILES
        "${PROJECT_SOURCE_DIR}/include/**/*.hpp"
        "${PROJECT_SOURCE_DIR}/include/**/*.hh"
        "${PROJECT_SOURCE_DIR}/include/**/*.h"
        "${PROJECT_SOURCE_DIR}/include/*.hpp"
        "${PROJECT_SOURCE_DIR}/include/*.hh"
        "${PROJECT_SOURCE_DIR}/include/*.h"
        "${PROJECT_SOURCE_DIR}/src/**/*.cpp"
        "${PROJECT_SOURCE_DIR}/src/**/*.cc"
        "${PROJECT_SOURCE_DIR}/src/**/*.c"
        "${PROJECT_SOURCE_DIR}/src/*.cpp"
        "${PROJECT_SOURCE_DIR}/src/*.cc"
        "${PROJECT_SOURCE_DIR}/src/*.c"
    )

    doxygen_add_docs(
        ${PROJECT_NAME}_documentation
        ${PROJECT_SOURCE_DIR}/README.md
        ${PROJECT_SOURCE_DIR}/LICENSE.md
        ${ALL_PROJECT_FILES}
        COMMENT "Generating Doxygen documentation for the library"
    )

endif()

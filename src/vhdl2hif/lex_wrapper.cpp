/// @file lex_wrapper.cpp
/// @brief
/// Copyright (c) 2024-2025, Electronic Systems Design (ESD) Group,
/// Univeristy of Verona.
/// This file is distributed under the BSD 2-Clause License.
/// See LICENSE.md for details.

#ifdef __clang__
#pragma clang diagnostic ignored "-Wconversion"
#pragma clang diagnostic ignored "-Wold-style-cast"
#pragma clang diagnostic ignored "-Wswitch-default"
#pragma clang diagnostic ignored "-Wunused-label"
#pragma clang diagnostic ignored "-Wunused-function"
#pragma clang diagnostic ignored "-Wpragmas"
#pragma clang diagnostic ignored "-Wzero-as-null-pointer-constant"
#pragma clang diagnostic ignored "-Wnull-dereference"
#elif defined __GNUC__
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wold-style-cast"
#pragma GCC diagnostic ignored "-Wswitch-default"
#pragma GCC diagnostic ignored "-Wunused-label"
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wpragmas"
#pragma GCC diagnostic ignored "-Wuseless-cast"
#pragma GCC diagnostic ignored "-Wzero-as-null-pointer-constant"
#pragma GCC diagnostic ignored "-Wnull-dereference"
#pragma GCC diagnostic ignored "-Wsign-compare"
#pragma GCC diagnostic ignored "-Wregister"
// not mingw
#ifndef _MSC_VER
#pragma GCC diagnostic ignored "-Wsign-conversion"
#endif
#else
#pragma warning(disable : 4127)
#pragma warning(disable : 4244)
#pragma warning(disable : 4505)
#pragma warning(disable : 4996)
#endif

#ifdef __clang__
#pragma clang diagnostic ignored "-Wimplicit-fallthrough"
#pragma clang diagnostic ignored "-Wdeprecated-register"
#endif

#include "vhdl2hif/vhdlLexer.cpp"
#include "vhdl2hif/vhdl_parser_struct.hpp"

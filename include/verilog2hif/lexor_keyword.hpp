/// @file lexor_keyword.hpp
/// @brief Lexor keyword header file.
/// Copyright (c) 2024-2025, Electronic Systems Design (ESD) Group,
/// Univeristy of Verona.
/// This file is distributed under the BSD 2-Clause License.
/// See LICENSE.md for details.

#pragma once

#ifdef HAVE_CVS_IDENT
#ident "$Id: lexor_keyword.h,v 1.2 2002/08/12 01:34:59 steve Exp $"
#endif

/// @brief Lexor keyword code.
/// @param str The keyword string.
/// @param len The keyword length.
/// @return The keyword code.
extern auto lexor_keyword_code(const char *str, unsigned len) -> int;

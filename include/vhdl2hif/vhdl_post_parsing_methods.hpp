/// @file vhdl_post_parsing_methods.hpp
/// @brief
/// Copyright (c) 2024-2025, Electronic Systems Design (ESD) Group,
/// Univeristy of Verona.
/// This file is distributed under the BSD 2-Clause License.
/// See LICENSE.md for details.

#pragma once

#include <hif/hif.hpp>

void performRangeRefinements(hif::System *o, bool use_int_32, hif::semantics::VHDLSemantics *sem);

void performStep1Refinements(hif::System *o, hif::semantics::ILanguageSemantics *sem);

void performStep2Refinements(hif::System *o, hif::semantics::ILanguageSemantics *sem);

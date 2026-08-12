/// @file post_parsing_methods.hpp
/// @brief Contains the declarations of the functions that perform the post-parsing refinements on the HIF AST.
/// Copyright (c) 2024-2025, Electronic Systems Design (ESD) Group,
/// Univeristy of Verona.
/// This file is distributed under the BSD 2-Clause License.
/// See LICENSE.md for details.

#pragma once

/// @brief Perform the first step of the post-parsing refinements.
/// @param o pointer to the system we are working on.
/// @param sem the semantic we are going to use.
void performStep1Refinements(hif::System *o, hif::semantics::ILanguageSemantics *sem);

/// @brief Perform the second step of the post-parsing refinements.
/// @param o pointer to the system we are working on.
/// @param sem the semantic we are going to use.
void performStep2Refinements(hif::System *o, hif::semantics::ILanguageSemantics *sem);

/// @brief Perform the third step of the post-parsing refinements.
/// @param o pointer to the system we are working on.
/// @param sem the semantic we are going to use.
/// @param preserveStructure if true, the structure of the AST is preserved.
void performStep3Refinements(hif::System *o, hif::semantics::ILanguageSemantics *sem, bool preserveStructure);

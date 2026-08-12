/// @file support.hpp
/// @brief Support for Verilog to HIF conversion.
/// Copyright (c) 2024-2025, Electronic Systems Design (ESD) Group,
/// Univeristy of Verona.
/// This file is distributed under the BSD 2-Clause License.
/// See LICENSE.md for details.

#pragma once

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <set>
#include <string>

// HIF library
#include <hif/hif.hpp>

#include "parser_struct.hpp"
#include "verilog_parser.hpp"

#ifdef __GNUC__
#pragma GCC diagnostic ignored "-Wsign-conversion"
#pragma GCC diagnostic ignored "-Wconversion"
#endif
#define HALT_ON_UNSUPPORTED_RULES

extern int yylineno;              ///< defined in verilogParser.cc
extern int yycolumno;             ///< defined in verilog_support.cc
extern std::string yyfilename;    ///< defined in verilog_support.cc
extern std::ostream *msgStream;   ///< defined in verilog2hif.cc
extern std::ostream *errorStream; ///< defined in verilog2hif.cc
extern std::ostream *debugStream; ///< defined in verilog2hif.cc

/////////////////////////////////////////////////////////////////
// Properties used by fix description.
/////////////////////////////////////////////////////////////////

extern const char *PROPERTY_SENSITIVE_POS;

extern const char *PROPERTY_SENSITIVE_NEG;

extern const char *PROPERTY_TASK_NOT_AUTOMATIC;

extern const char *PROPERTY_GENVAR;

// Property related to Assign. Default case (if not set): blocking assignment,
// else (if set): non-blocking assignment.
extern const char *NONBLOCKING_ASSIGNMENT;
extern const char *IS_VARIABLE_TYPE;
extern const char *HIF_ALL_SENSITIVITY;
extern const char *INITIAL_STATEMENT;

/////////////////////////////////////////////////////////////////
// Functions.
/////////////////////////////////////////////////////////////////

/// @brief This function build an Int that respects the verilog semantics.
/// @return a pointer to the newly created object.
auto makeVerilogIntegerType() -> hif::Bitvector *;

/// @brief This function builds a Bit that respects the verilog semantics.
/// @return a pointer to the newly created object.
auto makeVerilogBitType() -> hif::Bit *;

/// @brief This function builds an Array that represent a verilog reg. It is an
/// array unsigned, packed with type bit logic. The range given is not copied.
/// @param range the range of the array.
/// @return a pointer to the newly created object.
auto makeVerilogRegisterType(hif::Range *range = nullptr) -> hif::Bitvector *;

/// @brief This function builds an Array that represent a verilog reg signed. It
/// is an array unsigned, packed with type bit logic. The range given is not
/// copied.
/// @param range the range of the array.
/// @return a pointer to the newly created object.
auto makeVerilogSignedRegisterType(hif::Range *range = nullptr) -> hif::Bitvector *;

/// @brief This function builds a concatenation for all the values inside the
/// given list. The list must be not empty. The values provided are copied.
/// @param values the list of values to concatenate.
/// @return a pointer to the newly created object.
auto concat(hif::BList<hif::Value> &values) -> hif::Value *;

/// @brief Provides the semantic type of the given range.
/// @param ro the range to consider.
/// @param is_signed if true the type is signed, otherwise unsigned.
/// @return a pointer to the type.
auto getSemanticType(hif::Range *ro, bool is_signed = false) -> hif::Type *;

/// @brief Converts the given value to a bitvector.
/// @param number the value to convert.
/// @param numbit the number of bits of the bitvector.
/// @return A string representing the bitvector.
auto convertToBinary(const std::string &number, int numbit) -> std::string;

/// @brief Converts the given value to a bitvector.
/// @param number the value to convert.
/// @param numbit the number of bits of the bitvector.
/// @return A string representing the bitvector.
auto convertToBinary(int number, int numbit) -> std::string;

/// @brief Given a Switchalt of a casez or casex, transforms z & x into dontcare.
/// @param c the Switchalt to clean.
/// @param x if true, x is transformed into dontcare.
void clean_bitvalues(hif::SwitchAlt *c, bool x);

/// @brief Casts a BList of Parent_t to a BList of Child_t.
/// @tparam Child_t the type of the child.
/// @tparam Parent_t the type of the parent.
/// @param p list to cast.
/// @return the casted list.
template <typename Child_t, typename Parent_t> auto blist_scast(hif::BList<Parent_t> *p) -> hif::BList<Child_t> *
{
    // Parent_t * tmp_p = nullptr;
    // Child_t * tmp_c = nullptr;
    // tmp_c = static_cast<Child_t *>( tmp_p );
    return reinterpret_cast<hif::BList<Child_t> *>(p);
}

/// @brief A debug function that prints a given message.
/// @param msg the message to print.
void yyerror(VerilogParser *, const char *msg);

/// @brief A debug function that prints a given message.
/// @param msg the message to print.
/// @param o the object to print.
void yyerror(const char *msg, hif::Object *o = nullptr);

/// @brief A debug function that prints a given message.
/// @param msg the message to print.
/// @param o the object to print.
void yywarning(const char *msg, hif::Object *o = nullptr);

/// @brief A debug function that prints a given message.
/// @param msg the message to print.
/// @param o the object to print.
void yydebug(const char *msg, hif::Object *o = nullptr);

/// @brief A debug function that prints a given message.
/// @param msg the message to print.
void yymessage(const char *msg);

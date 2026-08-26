/// @file verilog_support.cpp
/// @brief
/// Copyright (c) 2024-2025, Electronic Systems Design (ESD) Group,
/// Univeristy of Verona.
/// This file is distributed under the BSD 2-Clause License.
/// See LICENSE.md for details.

#include <algorithm>

#include "verilog2hif/support.hpp"

/*
 * Global definitions used by vhdl_support, parser and lexer
 * --------------------------------------------------------------------- */
std::string yyfilename;
int yycolumno = 1;

using namespace hif;
using std::cout;
using std::endl;
using std::string;

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-noreturn"
#endif

/////////////////////////////////////////////////////////////////
// Initializations.
/////////////////////////////////////////////////////////////////

const char *HIF_ALL_SENSITIVITY    = "__HIF_ALL_SENSITIVITY";
const char *NONBLOCKING_ASSIGNMENT = "NONBLOCKING_ASSIGNMENT";
const char *IS_VARIABLE_TYPE       = "IS_VARIABLE_TYPE";

const char *PROPERTY_SENSITIVE_POS      = "pos";
const char *PROPERTY_SENSITIVE_NEG      = "neg";
const char *PROPERTY_TASK_NOT_AUTOMATIC = "PROPERTY_TASK_NOT_AUTOMATIC";
const char *PROPERTY_GENVAR             = "PROPERTY_GENVAR";
const char *PROPERTY_PROCESS_LOOP_TAIL  = "PROPERTY_PROCESS_LOOP_TAIL";

/////////////////////////////////////////////////////////////////
// Functions.
/////////////////////////////////////////////////////////////////

auto makeVerilogIntegerType() -> Bitvector *
{
    auto *bv = new Bitvector();
    bv->setSpan(new Range(31, 0));
    bv->setSigned(true);
    bv->setLogic(true);
    bv->setResolved(true);
    bv->setConstexpr(false);

    return bv;
}

auto makeVerilogBitType() -> Bit *
{
    Bit *b = new Bit();
    b->setLogic(true);
    b->setResolved(true);
    return b;
}

auto makeVerilogRegisterType(Range *range) -> Bitvector *
{
    auto *array = new Bitvector();
    array->setSpan(range);
    array->setSigned(false);
    array->setLogic(true);
    array->setResolved(true);
    return array;
}

auto makeVerilogSignedRegisterType(Range *range) -> Bitvector *
{
    Bitvector *array = makeVerilogRegisterType(range);
    array->setSigned(true);
    return array;
}

auto concat(BList<Value> &values) -> Value *
{
    messageAssert(!values.empty(), "Unexpected empty list values", nullptr, nullptr);

    // otherwise concatenations are needed
    BList<Value>::iterator it = values.begin();
    Value *curr_val           = hif::copy(*it);
    for (++it; it != values.end(); ++it) {
        auto *expr = new Expression();
        expr->setValue1(curr_val);
        expr->setValue2(hif::copy(*it));
        expr->setOperator(op_concat);
        curr_val = expr;
    }
    return curr_val;
}

/*
 * Converts the decimal number in the binary rappresentation. The function
 * returns the binary rappresentation as char pointer.
 *
 * @param num       decimal number rappresentation
 * @param numbit    number of bits representing the int value
 * @return the bynary representation
 */
auto convertToBinary(const string &number, int numBits) -> string
{
    int temp = atoi(number.c_str());
    return convertToBinary(temp, numBits);
}

auto convertToBinary(int number, int numBits) -> string
{
    int count = 0;
    string binaryNumber;

    while (count < numBits) {
        if ((number % 2) == 0) {
            binaryNumber.append("0");
        } else {
            binaryNumber.append("1");
        }

        number = number / 2;
        count++;
    }

    std::reverse(binaryNumber.begin(), binaryNumber.end());
    return binaryNumber;
}

void clean_bitvalues(SwitchAlt *c, bool x)
{
    for (BList<Value>::iterator i = c->conditions.begin(); i != c->conditions.end(); ++i) {
        // Bit:
        if (dynamic_cast<BitValue *>(*i) != nullptr) {
            auto *b = dynamic_cast<BitValue *>(*i);
            switch (b->getValue()) {
            case bit_x:
                b->setValue(x ? bit_dontcare : bit_x);
                break;
            case bit_z:
                b->setValue(bit_dontcare);
                break;
            case bit_zero:
            case bit_one:
            case bit_u:
            case bit_l:
            case bit_w:
            case bit_h:
            case bit_dontcare:
            default:;
            }
        }

        // Bitvector
        if (dynamic_cast<BitvectorValue *>(*i) != nullptr) {
            auto *b                          = dynamic_cast<BitvectorValue *>(*i);
            std::string v                    = b->getValue();
            const std::string::size_type len = v.size();
            for (std::string::size_type s = 0; s < len; ++s) {
                switch (v[s]) {
                case bit_x:
                    v[s] = x ? '-' : 'X';
                    break;
                case bit_z:
                    v[s] = '-';
                    break;
                case bit_zero:
                case bit_one:
                case bit_u:
                case bit_l:
                case bit_w:
                case bit_h:
                case bit_dontcare:
                default:;
                }
            }
            b->setValue(v);
        }
    }
}

auto getSemanticType(Range *ro, bool is_signed) -> Type *
{
    Type *to = nullptr;

    if (ro != nullptr) {
        Bitvector *ao = makeVerilogRegisterType(hif::copy(ro));
        ao->setSigned(is_signed);
        to = ao;
    } else {
        if (is_signed) {
            yywarning("Signed directive is ignored on single bits.");
        }
        Bit *bo = makeVerilogBitType();
        to      = bo;
    }

    return to;
}

//
//  Parser Output functions
//
#ifdef HALT_ON_UNSUPPORTED_RULES
void yyerror(char const *msg, Object *o)
{
    assert(msg != nullptr);
    (*errorStream) << " -- ERROR: " << msg << '\n';
    (*errorStream) << "    File: " << yyfilename.c_str() << " At line " << yylineno << ", column " << yycolumno << '\n';

    if (o != nullptr) {
        hif::writeFile(*errorStream, o, false);
        *errorStream << '\n';
    }
    assert(false);
    exit(1);
}
#else
void yyerror(char const *msg, Object *o)
{
    if (std::strncmp(msg, "syntax error", 12) == 0 || std::strncmp(msg, "Macro not found", 15) == 0 ||
        std::strncmp(msg, "File not found: ", 16) == 0 || std::strncmp(msg, "Unsupported directive", 21) == 0) {
        (*errorStream) << " -- ERROR: " << msg << endl;
        (*errorStream) << "    File: " << yyfilename.c_str() << " At line " << yylineno << ", column " << yycolumno
                       << endl;
        if (o != nullptr) {
            hif::writeFile(*errorStream, o, false);
            *errorStream << std::endl;
        }
        assert(false);
        exit(1);
    }
}
#endif

void yyerror(VerilogParser * /*unused*/, const char *msg) { yyerror(msg, nullptr); }

void yywarning(char const *msg, Object *o)
{
    assert(msg != nullptr);
    (*errorStream) << " -- WARNING: " << msg << " At line " << yylineno << ", column " << yycolumno << '\n';

    if (o != nullptr) {
        hif::writeFile(*errorStream, o, false);
        *errorStream << '\n';
    }
}

void yydebug(char const *msg, Object *o)
{
    assert(msg != nullptr);
    (*debugStream) << " -- DEBUG: " << msg << " At line " << yylineno << ", column " << yycolumno << '\n';

    if (o != nullptr) {
        hif::writeFile(*debugStream, o, false);
        *debugStream << '\n';
    }
}

void yymessage(const char *msg) { cout << "-- INFO: " << msg << " Line:" << yylineno << '\n'; }

/// @file PostParsingVisitor_fixRanges.cpp
/// @brief
/// Copyright (c) 2024-2025, Electronic Systems Design (ESD) Group,
/// Univeristy of Verona.
/// This file is distributed under the BSD 2-Clause License.
/// See LICENSE.md for details.

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>

#include "vhdl2hif/vhdl_parser.hpp"
#include "vhdl2hif/vhdl_post_parsing_methods.hpp"

using namespace hif;
using std::clog;
using std::endl;

namespace /*anon*/
{
class PostParsingVisitor_fixRanges : public hif::GuideVisitor
{
public:
    PostParsingVisitor_fixRanges(bool useInt32, hif::semantics::VHDLSemantics *sem);
    ~PostParsingVisitor_fixRanges() override;

    PostParsingVisitor_fixRanges(const PostParsingVisitor_fixRanges &o)                     = delete;
    auto operator=(const PostParsingVisitor_fixRanges &o) -> PostParsingVisitor_fixRanges & = delete;

    auto visitFunctionCall(hif::FunctionCall &o) -> int override;
    auto visitInt(hif::Int &o) -> int override;
    auto visitLibrary(Library &o) -> int override;

private:
    auto _fixTypeAttributeCall(FunctionCall *call, Type *t) -> Value *;

    bool _useInt32;
    hif::semantics::VHDLSemantics *_sem;
    hif::application_utils::WarningStringSet _librarySet;
    hif::Trash _trash;
};

PostParsingVisitor_fixRanges::PostParsingVisitor_fixRanges(const bool useInt32, hif::semantics::VHDLSemantics *sem)
    : hif::GuideVisitor()
    , _useInt32(useInt32)
    , _sem(sem)
    , _librarySet()
    , _trash()
{
    hif::application_utils::initializeLogHeader("VHDL2HIF", "PostParsingVisitor_fixRanges");
}

PostParsingVisitor_fixRanges::~PostParsingVisitor_fixRanges()
{
    messageWarningList(
        true,
        "Moving following nested packages not included within default "
        "library \"work\" into library \"work\". This may cause name"
        " conflicts.",
        _librarySet);

    _trash.clear();
    hif::application_utils::restoreLogHeader();
}

auto PostParsingVisitor_fixRanges::visitFunctionCall(FunctionCall &o) -> int
{
    GuideVisitor::visitFunctionCall(o);

    // Fix eventual attribute calls.
    // Those calls should be already mapped in functionCall by the semantics
    // (should not be mapped as member) and is assumed that them types are
    // fixed (not typed range or simila).
    // E.g. integer'low.

    if (dynamic_cast<Identifier *>(o.getInstance()) == nullptr) {
        return 0;
    }

    auto *id = dynamic_cast<Identifier *>(o.getInstance());
    std::string idString(id->getName());
    Type *idT = nullptr;
    idT       = VhdlParser::resolveType(idString, nullptr, nullptr, _sem, false);
    if (idT == nullptr) {
        return 0;
    }

    // fix type
    // Must be in tree to be able to call the guide visit.
    //idT->acceptVisitor(*this);

    // fix call
    Value *v = _fixTypeAttributeCall(&o, idT);
    if (v == nullptr) {
        delete idT;
        messageError("Unsupported attribute call on VHDL type", &o, _sem);
    }

    o.replace(v);
    delete &o;
    delete idT;
    return 0;
}

auto PostParsingVisitor_fixRanges::visitInt(Int &o) -> int
{
    GuideVisitor::visitInt(o);
    bool isSigned = o.isSigned();
    o.setSigned(true);
    Range *range = o.getSpan();
    Range *span  = nullptr;

    if (range == nullptr) {
        span = new Range(31, 0);
    } else //if( range != nullptr )
    {
        if (_useInt32) {
            span = new Range(31, 0);
        } else {
            span = hif::manipulation::transformRangeToSpan(range, _sem, isSigned);
            messageAssert(span != nullptr, "Cannot trasform span to range", range, _sem);
        }
    }
    o.setSpan(span);

    if (range == nullptr) {
        return 0;
    }
    if (dynamic_cast<ConstValue *>(o.getParent()) != nullptr) {
        // Is the syntactic type of const value.
        delete range;
        return 0;
    }

    if (dynamic_cast<Function *>(o.getParent()) != nullptr) {
        // Is the return type of function.
        delete range;
        return 0;
    }

    if (dynamic_cast<Cast *>(o.getParent()) != nullptr) {
        // Is the type of cast.
        delete range;
        return 0;
    }

    if (dynamic_cast<Range *>(o.getParent()) != nullptr) {
        // Is the type inside a typed range.
        // e.g.: type type_int2_32 is array (natural range <>) of integer range 2 to 32;
        // the natural range must be skipped
        delete range;
        return 0;
    }

    auto *pdecl = hif::getNearestParent<DataDeclaration>(&o);
    auto *tdecl = hif::getNearestParent<TypeDef>(&o);
    if (pdecl != nullptr) {
        messageDebugAssert(pdecl->getRange() == nullptr, "Unexpected range already set", pdecl, _sem);
        pdecl->setRange(range);
    } else if (tdecl != nullptr) {
        // Example crc_pkg.vhd:
        // type type_int2_32   is array (natural range <>) of integer range 2 to 32;
        // tdecl->GetRange() is Range (UPTO) (0) (2.....7)
        // Results:
        // 1) Should be inside array span (that is empty now)
        // 2) Should be typed

#ifndef NDEBUG
        if (tdecl->getRange() != nullptr) {
            hif::writeFile(clog, tdecl, false);
            clog << '\n';
            assert(false);
        }
#endif
        delete tdecl->setRange(range);
    } else

    {
        // could be a typedef?
        messageError("Unexpected case", nullptr, _sem);
    }

    return 0;
}

auto PostParsingVisitor_fixRanges::visitLibrary(Library &o) -> int
{
    // GuideVisitor::visitLibrary(o);

    // NOTE: possible "work" instance is dropped by the parser!

    if (o.getInstance() == nullptr) {
        // Special case: use work.module which is parsed as (LIBRARY module)
        if (!o.isInBList()) {
            return 0;
        }
        LibraryDef *ld = hif::semantics::getDeclaration(&o, _sem);
        if (ld != nullptr) {
            return 0;
        }

        ViewReference vr;
        vr.setDesignUnit(o.getName());
        hif::semantics::DeclarationOptions dopt;
        dopt.location = &o;
        View *view    = hif::semantics::getDeclaration(&vr, _sem, dopt);
        messageAssert(view != nullptr, "Unexpected missing declaration", &o, _sem);
        _trash.insert(&o);

        return 0;
    }

    auto *inst = dynamic_cast<Library *>(o.getInstance());
    messageAssert(inst != nullptr, "Unexpected library instance", &o, _sem);
    auto *lib = dynamic_cast<Library *>(inst->getInstance());

    bool instIEEE = inst->getName() == "ieee" || inst->getName() == "std";

    bool libIEEE = lib != nullptr && (lib->getName() == "ieee" || lib->getName() == "std");

    if (!instIEEE && !libIEEE) {
        if (lib == nullptr) {
            messageAssert(inst->getInstance() == nullptr, "Unexpected instance type.", &o, _sem);
            // 1) myPackage.foo   --> myPackage
            // 2) myLib.myPackage --> myPackage
            LibraryDef *ld = hif::semantics::getDeclaration(inst, _sem);
            if (ld != nullptr) {
                // case 1
                o.replace(inst);
                delete &o;
            } else {
                // case 2
                inst->replace(nullptr);
                delete inst;
            }
        } else {
            // myLib.myPackage.foo --> myPackage
            std::string res = std::string(lib->getName()) + "." + inst->getName() + "." + o.getName();
            _librarySet.insert(res);
            lib->replace(nullptr);
            delete lib;
            o.replace(inst);
            delete &o;
        }
        return 0;
    }

    // ieee or std. E.g.:
    // 1) ieee.std_logic_arith;
    // 2) ieee.std_logic_arith.CONV_STD_LOGIC_VECTOR

    if (lib == nullptr) {
        messageAssert(inst->getInstance() == nullptr, "Unexpected instance type.", &o, _sem);
        // case 1
        o.setName(std::string(inst->getName() + "_" + o.getName()));
        delete o.setInstance(nullptr);
        o.setSystem(true);
    } else {
        // case 2
        inst->setName(std::string(lib->getName() + "_" + inst->getName()));
        delete inst->setInstance(nullptr);
        inst->setSystem(true);
        o.replace(inst);
        delete &o;
    }

    return 0;
}

auto PostParsingVisitor_fixRanges::_fixTypeAttributeCall(FunctionCall *call, Type *t) -> Value *
{
    std::string callName = call->getName();

    Value *ret = nullptr;
    if (callName == "low" || (callName == "high")) {
        Range *range = hif::typeGetSpan(t, _sem);
        messageAssert(range != nullptr, "Span not found", t, _sem);
        messageAssert(
            range->getDirection() == dir_downto || range->getDirection() == dir_upto, "Unexpected range direction",
            range, _sem);
        Value *bound = nullptr;
        if (callName == "low") {
            bound = (range->getDirection() == dir_downto) ? range->getRightBound() : range->getLeftBound();
        } else {
            bound = (range->getDirection() == dir_downto) ? range->getLeftBound() : range->getRightBound();
        }
        ret = hif::copy(bound);
    } else if (callName == "left" || (callName == "right")) {
        Range *span = hif::typeGetSpan(t, _sem);
        messageAssert(span != nullptr, "Span not found", t, _sem);
        messageAssert(
            span->getDirection() == dir_downto || span->getDirection() == dir_upto, "Unexpected range direction", span,
            _sem);
        Value *bound = (callName == "left") ? span->getLeftBound() : span->getRightBound();
        ret          = hif::copy(bound);
    } else if (callName == "val") {
        Cast *v = new Cast();
        v->setType(hif::copy(t));
        messageAssert(call->parameterAssigns.size() == 1, "Expected only one parameter.", call, _sem);
        v->setValue(hif::copy(call->parameterAssigns.front()->getValue()));
        ret = v;
    } else {
        messageDebugAssert(false, "Unhandeld attribute call on type", call, _sem);
    }

    return ret;
}

// ///////////////////////////////////////////////////////////////////
// Utility methods
// ///////////////////////////////////////////////////////////////////

auto _hasSameSignagure(SubProgram *s1, SubProgram *s2) -> bool
{
    StateTable *s1st = s1->setStateTable(nullptr);
    StateTable *s2st = s2->setStateTable(nullptr);
    bool ret   = hif::equals(s1, s2);
    s1->setStateTable(s1st);
    s2->setStateTable(s2st);
    return ret;
}

void _fixSubProgramsDeclarations(System *o, hif::semantics::VHDLSemantics *sem)
{
    for (BList<LibraryDef>::iterator k = o->libraryDefs.begin(); k != o->libraryDefs.end(); ++k) {
        LibraryDef *l = *k;
        hif::Trash trash;
        if (l->isStandard()) {
            continue;
        }
        for (BList<Declaration>::iterator i = l->declarations.begin(); i != l->declarations.end(); ++i) {
            Declaration *d = *i;
            auto *sub_i    = dynamic_cast<SubProgram *>(d);
            if (sub_i == nullptr) {
                continue;
            }
            BList<Declaration>::iterator j = i;
            ++j;
            bool found        = false;
            SubProgram *sub_j = nullptr;
            for (; j != l->declarations.end(); ++j) {
                sub_j = dynamic_cast<SubProgram *>(*j);
                if (sub_j == nullptr) {
                    continue;
                }
                if (!_hasSameSignagure(sub_i, sub_j)) {
                    continue;
                }
                found = true;
                break;
            }

            if (!found) {
                continue;
            }

            if ((sub_i->getStateTable() != nullptr && sub_j->getStateTable() != nullptr) ||
                (sub_i->getStateTable() == nullptr && sub_j->getStateTable() == nullptr)) {
                messageAssert(
                    hif::equals(sub_i, sub_j), "Found different subprograms with conflicting declaration", sub_i, sem);
                trash.insert(sub_j);
            } else if (sub_i->getStateTable() != nullptr) {
                trash.insert(sub_j);
            } else // if (sub_j->getStateTable() != nullptr)
            {
                trash.insert(sub_i);
            }
        }

        trash.clear();
    }
}

} // namespace

void performRangeRefinements(hif::System *o, bool use_int_32, hif::semantics::VHDLSemantics *sem)
{
    hif::application_utils::initializeLogHeader("VHDL2HIF", "performRangeRefinements");

    _fixSubProgramsDeclarations(o, sem);

    PostParsingVisitor_fixRanges v(use_int_32, sem);
    o->acceptVisitor(v);

    hif::application_utils::restoreLogHeader();
}

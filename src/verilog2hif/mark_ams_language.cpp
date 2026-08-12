/// @file mark_ams_language.cpp
/// @brief
/// Copyright (c) 2024-2025, Electronic Systems Design (ESD) Group,
/// Univeristy of Verona.
/// This file is distributed under the BSD 2-Clause License.
/// See LICENSE.md for details.

#include "verilog2hif/mark_ams_language.hpp"

class AmsMarker : public hif::DeclarationVisitor
{
public:
    AmsMarker(const hif::DeclarationVisitorOptions &opts);
    ~AmsMarker() override;

    auto visitFunctionCall(hif::FunctionCall &o) -> int override;
    auto visitIdentifier(hif::Identifier &o) -> int override;
    auto visitProcedureCall(hif::ProcedureCall &o) -> int override;
    auto visitLibraryDef(hif::LibraryDef &o) -> int override;
    auto visitStateTable(hif::StateTable &o) -> int override;
    auto visitSystem(hif::System &o) -> int override;
    auto visitTypeReference(hif::TypeReference &o) -> int override;
    auto visitView(hif::View &o) -> int override;
    auto visitViewReference(hif::ViewReference &o) -> int override;

private:
    template <typename T> void _checkAmsSymbol(T *symbol);
    void _markScope(hif::Object *o);

    bool _isAms{false};

    AmsMarker(AmsMarker &)                           = delete;
    auto operator=(const AmsMarker &) -> AmsMarker & = delete;
};

AmsMarker::AmsMarker(const hif::DeclarationVisitorOptions &opts)
    : hif::DeclarationVisitor(opts)

{
    // ntd
}

AmsMarker::~AmsMarker()
{
    // ntd
}

auto AmsMarker::visitFunctionCall(hif::FunctionCall &o) -> int
{
    DeclarationVisitor::visitFunctionCall(o);
    _checkAmsSymbol(&o);
    return 0;
}

auto AmsMarker::visitIdentifier(hif::Identifier &o) -> int
{
    DeclarationVisitor::visitIdentifier(o);
    _checkAmsSymbol(&o);
    return 0;
}

auto AmsMarker::visitProcedureCall(hif::ProcedureCall &o) -> int
{
    DeclarationVisitor::visitProcedureCall(o);
    _checkAmsSymbol(&o);
    return 0;
}

auto AmsMarker::visitLibraryDef(hif::LibraryDef &o) -> int
{
    if (o.isStandard()) {
        return 0;
    }
    return DeclarationVisitor::visitLibraryDef(o);
}

auto AmsMarker::visitStateTable(hif::StateTable &o) -> int
{
    DeclarationVisitor::visitStateTable(o);
    if (o.getFlavour() != hif::pf_analog) {
        return 0;
    }
    _markScope(&o);
    return 0;
}

auto AmsMarker::visitSystem(hif::System &o) -> int
{
    DeclarationVisitor::visitSystem(o);
    if (_isAms) {
        o.setLanguageID(hif::ams);
    }
    return 0;
}

auto AmsMarker::visitTypeReference(hif::TypeReference &o) -> int
{
    DeclarationVisitor::visitTypeReference(o);
    _checkAmsSymbol(&o);
    return 0;
}

auto AmsMarker::visitView(hif::View &o) -> int
{
    if (o.isStandard()) {
        return 0;
    }
    return DeclarationVisitor::visitView(o);
}

auto AmsMarker::visitViewReference(hif::ViewReference &o) -> int
{
    DeclarationVisitor::visitViewReference(o);
    _checkAmsSymbol(&o);
    return 0;
}

template <typename T> void AmsMarker::_checkAmsSymbol(T *symbol)
{
    typename T::DeclarationType *decl = hif::semantics::getDeclaration(symbol, _opt.sem);
    messageAssert(decl != nullptr, "Declaration not found", symbol, _opt.sem);
    if (hif::objectGetLanguage(decl) != hif::ams) {
        return;
    }
    _markScope(symbol);
}

void AmsMarker::_markScope(hif::Object *o)
{
    auto *v = hif::getNearestParent<hif::View>(o);
    if (v != nullptr) {
        v->setLanguageID(hif::ams);
    }
    auto *ld = hif::getNearestParent<hif::LibraryDef>(o);
    if (ld != nullptr) {
        ld->setLanguageID(hif::ams);
    }
    //    hif::System * sys = hif::getNearestParent<hif::System>(o);
    //    if (sys != nullptr) sys->setLanguageID(hif::ams);

    _isAms = true;
}

void markAmsLanguage(hif::Object *o, hif::semantics::ILanguageSemantics *sem)
{
    hif::DeclarationVisitorOptions opt;
    opt.sem                             = sem;
    opt.visitDeclarationsOnce           = true;
    opt.visitSymbolsOnce                = true;
    opt.visitReferencesAfterDeclaration = false;
    AmsMarker vis(opt);
    o->acceptVisitor(vis);
}

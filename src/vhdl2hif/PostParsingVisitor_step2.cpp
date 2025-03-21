/// @file PostParsingVisitor_step2.cpp
/// @brief
/// Copyright (c) 2024-2025, Electronic Systems Design (ESD) Group,
/// Univeristy of Verona.
/// This file is distributed under the BSD 2-Clause License.
/// See LICENSE.md for details.

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>

#include <hif/hif.hpp>

#include "vhdl2hif/vhdl_post_parsing_methods.hpp"
#include "vhdl2hif/vhdl_support.hpp"

#ifdef __GNUC__
#pragma GCC diagnostic warning "-Wdisabled-optimization"
#endif

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-member-function"
#elif defined __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#endif

using namespace hif;

namespace
{ // anon namespace

using Partials     = std::set<PortAssign *>;
using PartialNames = std::map<std::string, Partials>;

auto _getOverloadedFunctionName(Operator oper) -> std::string
{
    std::string fname("__vhdl_op_");

    switch (oper) {
    case op_bor:
        return fname + "bor";
    case op_band:
        return fname + "band";
    case op_bxor:
        return fname + "bxor";
    case op_bnot:
        return fname + "bnot";
    case op_eq:
        return fname + "eq";
    case op_neq:
        return fname + "neq";
    case op_le:
        return fname + "le";
    case op_ge:
        return fname + "ge";
    case op_lt:
        return fname + "lt";
    case op_gt:
        return fname + "gt";
    case op_sll:
        return fname + "sll";
    case op_srl:
        return fname + "srl";
    case op_sla:
        return fname + "sla";
    case op_sra:
        return fname + "sra";
    case op_rol:
        return fname + "rol";
    case op_ror:
        return fname + "ror";
    case op_plus:
        return fname + "plus";
    case op_minus:
        return fname + "minus";
    case op_mult:
        return fname + "mult";
    case op_div:
        return fname + "div";
    case op_mod:
        return fname + "mod";
    case op_rem:
        return fname + "rem";
    case op_pow:
        return fname + "pow";
    case op_abs:
        return fname + "abs";
    case op_concat:
        return fname + "concat";

    case op_case_eq:
    case op_case_neq:
    case op_xor:
    case op_not:
    case op_and:
    case op_or:
    case op_none:
    case op_ref:
    case op_deref:
    case op_andrd:
    case op_orrd:
    case op_xorrd:
    case op_assign:
    case op_conv:
    case op_bind:
    case op_log:
    case op_reverse:
    case op_size:
    default:
        break;
    }

    return "";
    //return fname;
}

/// This visitor is called at the end of the parsing of a VHDL file
///
/// 1- It sets the libraries used in the description
/// 2- Establish if operator is bitwise or logical basing on semantics analysis
/// 3- Fix the SIGNED() and UNSIGNED() cast with no range
/// 4- Remove spurious instances
/// 5- Fix ports not assigned in instance objects
/// 6- Removes useless library inclusion referring to design units
/// 7- Fix all kind of DeclarationObjects (different from DesignUnit). If
///     their name matches the name of DU, change the corresponding declaration
///     and all the references to it.
class PostParsingVisitor_step2 : public hif::GuideVisitor
{
public:
    using Operators = std::set<SubProgram *>;

    PostParsingVisitor_step2(hif::semantics::ILanguageSemantics *sem);
    ~PostParsingVisitor_step2() override;

    auto visitAggregate(hif::Aggregate &o) -> int override;
    auto visitBitvectorValue(hif::BitvectorValue &o) -> int override;
    auto visitCast(hif::Cast &o) -> int override;
    auto visitExpression(hif::Expression &o) -> int override;
    auto visitFor(hif::For &o) -> int override;
    auto visitForGenerate(hif::ForGenerate &o) -> int override;
    auto visitFunctionCall(hif::FunctionCall &o) -> int override;
    auto visitInstance(hif::Instance &o) -> int override;     // previsit fix
    auto visitLibraryDef(hif::LibraryDef &o) -> int override; // previsit fix
    auto visitProcedureCall(hif::ProcedureCall &o) -> int override;
    auto visitSlice(hif::Slice &o) -> int override;
    auto visitStateTable(StateTable &o) -> int override;
    auto visitSwitch(hif::Switch &o) -> int override;
    auto visitSystem(hif::System &o) -> int override; // previsit fix

private:
    auto operator=(const PostParsingVisitor_step2 &o) -> PostParsingVisitor_step2 & = delete;
    PostParsingVisitor_step2(const PostParsingVisitor_step2 &o)                     = delete;

    hif::semantics::ILanguageSemantics *_sem;
    hif::HifFactory _factory;

    Operators _operators;

    /// @brief Fix the index of the hif::For, hif::ForGenerate
    ///
    /// Note: in case of ForGenerate some fixes are not performed, assuming a
    /// simple and well-formed statement.
    ///
    /// In HIF the hif::For, hif::ForGenerate index may be defined within a
    /// range of values. If this range is equal to the index variable's span,
    /// with a straight translation we get an overflow of the index variable.
    ///
    /// For example, suppose to have an index declared as:
    /// @code
    /// sc_uint<4> i;
    /// @endcode
    /// and a for that uses this index variables, assigning all its values.
    /// Translating the for as:
    /// @code
    /// for (i = 0; i <= 15; ++i) {
    ///     ...
    /// }
    /// @endcode
    /// We get an infinite loop, because after the "last" iteration, in which
    /// i = 15 (its maximum value), the loop increments the index value, getting
    /// i = 0 (and not i = 16) because of an overflow.
    /// Other solutions like:
    /// @code
    /// for (i = 0; i < 15; ++i)
    /// @endcode
    /// will not work because the index in this case does not assume all the
    /// values of the range (i = 15 is not used).
    /// Changing the declaration of the index could cause many problems, because
    /// it can be used in other parts of the design (that assumes a certain type
    /// of span for this index).
    /// The solution adopted here is to add a support variable for the loop in
    /// this way:
    /// @code
    /// // in the declarations
    /// sc_uint<4> i;
    /// sc_uint<5> i_support;   // added this declaration
    ///
    /// // in the for
    /// i = 0;
    /// i_support = 0;
    /// for (; i_support <= 15; ++i_support) {
    ///     i = i_support;
    ///     ...
    /// }
    /// @endcode
    /// Avoiding overflows.
    ///
    /// @{
    auto _manageLoopIndex(hif::Value *condition) -> hif::Value *;
    template <typename T> void _manageForIndex(T &o);

    auto _transformRangeToExpression(hif::BList<hif::DataDeclaration> &initDeclarations, hif::Value *condition)
        -> hif::Value *;
    void _manageForRange(hif::For &o);
    void _manageForGenerateRange(hif::ForGenerate &o);
    /// @}

    /// @brief Ensures that all declarations related to hif::For index(es) exist.
    void _manageForDeclarations(hif::For &o);

    /// @name Expression-related fixes.
    /// @{
    /// @brief Transforms overloaded operators to relative function calls.
    /// Replaces directly the expression into the tree.
    auto _fixOverloadedOperators(hif::Expression &o) -> bool;
    static auto _isOverloadable(hif::Operator op, Type *t1, Type *t2) -> bool;
    void _addStandardOperatorOverloads(LibraryDef *o);
    void _removeStandardOperatorOverloads();
    void _parseOperator(
        LibraryDef *ld,
        const std::string &op,
        const std::string &param1,
        const std::string &param2,
        const std::string &param3 = std::string());
    void _makeParam(Function *sub, const std::string &pos, const std::string &type);
    void _makeReturnType(Function *sub, const std::string &type);
    auto _makeType(Function *sub, const std::string &type, const std::string &pos) -> Type *;

    /// @}

    /// @name Cast-related fixes.
    /// @{
    void _manageCast(Cast &o);
    auto _getCastedType(Cast &o) -> Type *;
    /// @}

    /// @name Instance-related fixes.
    /// @{
    /// Compose a concat operation composing partial bindings of each PortAssign
    auto _fixPartialBindings(Instance *inst) -> bool;

    /// Create a new PortAssign composing partial bindings related to one Port
    void _fixPortPartialBindings(Partials &partials);
    auto _getPartial(Value *index, Value *min, hif::Trash &trash, bool hasTemplates) -> Value *;
    auto _getPartial(Range *index, Value *min, hif::Trash &trash, bool hasTemplates) -> Range *;

    /// @}

    /// @name Attributes fix related methods
    /// @{
    auto _fixAttributesDimension(FunctionCall *o) -> bool;
    auto _simplifyAttributes(FunctionCall *o) -> bool;
    auto _checkTypeInstance(Object *o) -> Type *;
    /// @}
};

PostParsingVisitor_step2::PostParsingVisitor_step2(hif::semantics::ILanguageSemantics *sem)
    : _sem(sem)
    , _factory(sem)
    , _operators()
{
    // ntd
}

PostParsingVisitor_step2::~PostParsingVisitor_step2() { _removeStandardOperatorOverloads(); }

auto PostParsingVisitor_step2::visitLibraryDef(LibraryDef &o) -> int
{
    auto *sys = dynamic_cast<System *>(o.getParent());
    messageAssert(sys != nullptr, "Cannot find system", &o, _sem);
    for (BList<Declaration>::iterator i = o.declarations.begin(); i != o.declarations.end(); ++i) {
        auto *du = dynamic_cast<DesignUnit *>(*i);
        if (du == nullptr) {
            continue;
        }
        for (BList<DesignUnit>::iterator j = sys->designUnits.begin(); j != sys->designUnits.end(); ++j) {
            if (du->getName() == (*j)->getName()) {
                // Port references may be set to the wrong declarations, and they
                // are going to be invalid. Ref.design: ips/spi2
                hif::semantics::resetDeclarations(*j);

                DesignUnit *tmp = hif::copy(*j);
                j.erase();
                du->replace(tmp);
                break;
            }
        }
    }

    return GuideVisitor::visitLibraryDef(o);
}

auto PostParsingVisitor_step2::visitExpression(Expression &o) -> int
{
    GuideVisitor::visitExpression(o);

    if (_fixOverloadedOperators(o)) {
        return 0;
    }

    return 0;
}

auto PostParsingVisitor_step2::visitCast(Cast &o) -> int
{
    GuideVisitor::visitCast(o);

    _manageCast(o);

    return 0;
}

auto PostParsingVisitor_step2::visitFor(For &o) -> int
{
    //  _manageForDeclarations( o );

    GuideVisitor::visitFor(o);

    // Check the presence of a range. If present, it has been left to check a possible
    // overflow. After the fix, it will be transformed into an expression.
    auto *forRange = dynamic_cast<Range *>(o.getCondition());
    if (forRange != nullptr) {
        _manageForIndex(o);
        _manageForRange(o);
    } else {
        messageDebugAssert(false, "Unexpected for", &o, _sem);
    }

    return 0;
}

auto PostParsingVisitor_step2::visitForGenerate(ForGenerate &o) -> int
{
    //  _manageForDeclarations( o );

    GuideVisitor::visitForGenerate(o);

    // Check the presence of a range. If present, it has been left to check a possible
    // overflow. After the fix, it will be transformed into an expression.
    auto *forRange = dynamic_cast<Range *>(o.getCondition());
    if (forRange != nullptr) {
        _manageForIndex(o);
        _manageForGenerateRange(o);
    } else {
        messageDebugAssert(false, "Unexpected for generate", &o, _sem);
    }

    return 0;
}

auto PostParsingVisitor_step2::visitFunctionCall(hif::FunctionCall &o) -> int
{
    GuideVisitor::visitFunctionCall(o);

    //
    // Fix unnamed parameter assigns retrieving the correct name
    // from the function signature
    //
    FunctionCall::DeclarationType *decl = hif::semantics::getDeclaration(&o, _sem);
    if (decl == nullptr) {
        messageError("Not found declaration of function call.", &o, _sem);
    }

    messageAssert(
        decl->parameters.size() >= o.parameterAssigns.size(),
        "Unexpected number of formal parameter greater than actuals", decl, _sem);

    hif::manipulation::sortParameters(
        o.parameterAssigns, decl->parameters, true, hif::manipulation::SortMissingKind::NOTHING, _sem);

    if (_fixAttributesDimension(&o)) {
        return 0;
    }
    if (_simplifyAttributes(&o)) {
        return 0;
    }

    return 0;
}

auto PostParsingVisitor_step2::_fixAttributesDimension(FunctionCall *o) -> bool
{
    std::string callName = o->getName();

    if (o->getInstance() == nullptr) {
        return false;
    }
    if (!o->parameterAssigns.empty()) {
        return false;
    }
    if (callName != "left" && callName != "right" && callName != "low" && callName != "high") {
        return false;
    }

    Type *typeInstance = _checkTypeInstance(o->getInstance());
    Type *t            = nullptr;
    if (typeInstance != nullptr) {
        o->replace(typeInstance);
        Type *base = hif::semantics::getBaseType(typeInstance, false, _sem);
        typeInstance->replace(o);
        t = base;
    }
    if (t == nullptr) {
        t = hif::semantics::getBaseType(hif::semantics::getSemanticType(o->getInstance(), _sem), false, _sem);
    }
    messageAssert(t != nullptr, "Cannot type function call instance", o->getInstance(), _sem);

    if (dynamic_cast<Bitvector *>(t) == nullptr && dynamic_cast<Signed *>(t) == nullptr &&
        dynamic_cast<Unsigned *>(t) == nullptr && dynamic_cast<Array *>(t) == nullptr) {
        delete typeInstance;
        return false;
    }
    o->parameterAssigns.push_back(_factory.parameterArgument("param1", _factory.intval(1)));

    delete typeInstance;
    hif::semantics::resetDeclarations(o);
    hif::semantics::resetTypes(o);
    o->acceptVisitor(*this);

    return true;
}

auto PostParsingVisitor_step2::_simplifyAttributes(FunctionCall *o) -> bool
{
    std::string callName = o->getName();

    if (o->getInstance() == nullptr) {
        return false;
    }
    if (callName != "left" && callName != "right" && callName != "low" && callName != "high") {
        return false;
    }

    Type *typeInstance = _checkTypeInstance(o->getInstance());
    Type *actualType   = nullptr;
    Type *baseType     = nullptr;
    if (typeInstance != nullptr) {
        o->replace(typeInstance);
        Type *base = hif::semantics::getBaseType(typeInstance, false, _sem);
        typeInstance->replace(o);
        baseType   = base;
        actualType = typeInstance;
    }
    if (baseType == nullptr) {
        actualType = hif::semantics::getSemanticType(o->getInstance(), _sem);
        baseType   = hif::semantics::getBaseType(actualType, false, _sem);
    }
    messageAssert(baseType != nullptr, "Cannot type function call intance", o->getInstance(), _sem);

    Range *span = nullptr;
    if (hif::semantics::isVectorType(baseType, _sem) || dynamic_cast<Array *>(baseType) != nullptr) {
        span = hif::typeGetSpan(baseType, _sem);
    } else {
        auto *tr = dynamic_cast<TypeReference *>(actualType);
        messageAssert(tr != nullptr, "Expected type reference", actualType, _sem);
        TypeDef *td = dynamic_cast<TypeDef *>(hif::semantics::getDeclaration(tr, _sem));
        messageAssert(td != nullptr, "Declaration not found", tr, _sem);
        if (td->getRange() != nullptr) {
            span = td->getRange();
        } else {
            span = hif::typeGetSpan(baseType, _sem);
        }
    }

    messageAssert(span != nullptr, "Cannot find span", baseType, _sem);
    if (span->getDirection() != dir_downto && span->getDirection() != dir_upto) {
        messageError("Unexpected span", span, _sem);
    }

    Value *bound = nullptr;
    if (callName == "left") {
        bound = span->getLeftBound();
    } else if (callName == "right") {
        bound = span->getRightBound();
    } else if (callName == "low") {
        bound = (span->getDirection() == dir_downto) ? span->getRightBound() : span->getLeftBound();
    } else if (callName == "high") {
        bound = (span->getDirection() == dir_downto) ? span->getLeftBound() : span->getRightBound();
    } else {
        messageError("Unexpected case", o, _sem);
    }

    Value *newBound = hif::copy(bound);
    o->replace(newBound);
    delete typeInstance;
    delete o;

    newBound->acceptVisitor(*this);
    return true;
}

auto PostParsingVisitor_step2::_checkTypeInstance(Object *o) -> Type *
{
    // Checking special instances.
    // E.g.: a'left: a can be a value or a type.
    // Parser matches both as Identifiers.
    auto *id = dynamic_cast<Identifier *>(o);
    if (id == nullptr) {
        return nullptr;
    }
    Declaration *decl = hif::semantics::getDeclaration(id, _sem);
    if (decl != nullptr) {
        return nullptr;
    }

    Type *t = VhdlParser::resolveType(id->getName(), nullptr, nullptr, _sem, true);
    messageAssert(t != nullptr, "Unable to check the instance.", o, _sem);
    return t;
}

auto PostParsingVisitor_step2::visitInstance(Instance &o) -> int
{
    _fixPartialBindings(&o);

    for (BList<PortAssign>::iterator it = o.portAssigns.begin(); it != o.portAssigns.end();) {
        if ((*it)->getValue() == nullptr) {
            // it is an open portassign. Remove it!
            it = it.erase();
        } else {
            ++it;
        }
    }

    // Sorting port and template parameters.
    messageDebugAssert(o.getReferencedType() != nullptr, "Unexpected nullptr referenced type", &o, _sem);
    auto *vref = dynamic_cast<ViewReference *>(o.getReferencedType());

    if (vref == nullptr) {
        return 0;
    }

    View *vr = hif::semantics::getDeclaration(vref, _sem);
    messageAssert(vr != nullptr, "Not found declaration of view reference.", vref, _sem);

    hif::manipulation::sortParameters(
        o.portAssigns, vr->getEntity()->ports, true, hif::manipulation::SortMissingKind::NOTHING, _sem);
    hif::manipulation::sortParameters(
        vref->templateParameterAssigns, vr->templateParameters, true, hif::manipulation::SortMissingKind::NOTHING,
        _sem);

    GuideVisitor::visitInstance(o);
    return 0;
}

auto PostParsingVisitor_step2::visitProcedureCall(hif::ProcedureCall &o) -> int
{
    GuideVisitor::visitProcedureCall(o);

    // Fix unnamed parameter assigns retrieving the correct name
    // from the procedure signature
    ProcedureCall::DeclarationType *decl = hif::semantics::getDeclaration(&o, _sem);
    if (decl == nullptr) {
        messageError("Not found declaration of procedure call.", &o, _sem);
    }

    messageAssert(
        decl->parameters.size() >= o.parameterAssigns.size(),
        "Unexpected number of formal parameter greater than actuals", decl, _sem);

    hif::manipulation::sortParameters(
        o.parameterAssigns, decl->parameters, true, hif::manipulation::SortMissingKind::NOTHING, _sem);

    return 0;
}

auto PostParsingVisitor_step2::visitAggregate(Aggregate &o) -> int
{
    GuideVisitor::visitAggregate(o);

    if (!o.checkProperty(AGGREGREGATE_INIDICES_PROPERTY)) {
        return 0;
    }
    o.removeProperty(AGGREGREGATE_INIDICES_PROPERTY);

    // ref design vhdl/openCores/corproc

    Type *aggType = hif::semantics::getSemanticType(&o, _sem);
    messageAssert(aggType != nullptr, "Cannot type aggregate", &o, _sem);

    Range *span = hif::typeGetSpan(aggType, _sem);
    messageAssert(span != nullptr, "Span not found", aggType, _sem);
    const RangeDirection dir = span->getDirection();

    const BList<Object>::size_t size = o.alts.size();
    BList<Object>::size_t pos        = 0;
    Value *min                       = hif::rangeGetMinBound(span);
    for (BList<AggregateAlt>::iterator it = o.alts.begin(); it != o.alts.end(); ++it) {
        AggregateAlt *alt = *it;
        alt->indices.clear();

        if (dir == dir_downto) {
            Expression *e = _factory.expression(
                hif::manipulation::assureSyntacticType(hif::copy(min), _sem), op_plus,
                _factory.intval(static_cast<long long>(size - pos - 1)));
            alt->indices.push_back(e);
        } else {
            Expression *e = _factory.expression(
                hif::manipulation::assureSyntacticType(hif::copy(min), _sem), op_plus,
                _factory.intval(static_cast<long long>(pos)));
            alt->indices.push_back(e);
        }
        ++pos;
    }

    return 0;
}

auto PostParsingVisitor_step2::visitBitvectorValue(hif::BitvectorValue &o) -> int
{
    GuideVisitor::visitBitvectorValue(o);

    // Reset type since it could depend by the context.
    Type *originalType = o.setType(nullptr);
    Range *newSpan     = hif::copy(hif::typeGetSpan(originalType, _sem));

    hif::semantics::resetTypes(&o, false);

    Type *t = hif::semantics::getOtherOperandType(&o, _sem, true, true);
    if (t == nullptr) {
        // Type seems to not be context-dependent,
        // i.e. more than a match has been found.
        // see vhdl/custom/root design.
        delete newSpan;
        o.setType(originalType);
        return 0;
    }

    //messageAssert(t != nullptr, "Unable to calculate BitvectorValue type", &o, _sem);
    //messageAssert(hif::semantics::isVectorType(t, _sem), "Expected vector type", t, _sem);
    if (dynamic_cast<String *>(t) != nullptr) {
        auto *strval = new StringValue();
        strval->setType(_sem->getTypeForConstant(strval));
        strval->setValue(o.getValue());
        o.replace(strval);
        delete &o;
        delete originalType;
        return 0;
    }

    if (!hif::semantics::isVectorType(t, _sem)) {
        o.setType(originalType);
        hif::typeSetSigned(originalType, hif::typeIsSigned(t, _sem), _sem);
    } else {
        messageAssert(newSpan != nullptr, "Expected span", originalType, _sem);
        o.setType(hif::copy(t));
        hif::typeSetSpan(o.getType(), newSpan, _sem);
        hif::typeSetConstexpr(o.getType(), true);
        delete originalType;
    }
    hif::semantics::resetTypes(&o, false);

    return 0;
}

auto PostParsingVisitor_step2::visitSlice(Slice &o) -> int
{
    GuideVisitor::visitSlice(o);

    Type *t = hif::semantics::getSemanticType(o.getPrefix(), _sem);
    messageAssert(t != nullptr, "Cannot type slice prefix", &o, _sem);

    Range *span = hif::typeGetSpan(t, _sem);
    if (span == nullptr) {
        return 0;
    }
    RangeDirection prefix_dir = span->getDirection();
    RangeDirection slice_dir  = o.getSpan()->getDirection();
    if (prefix_dir != slice_dir) {
        // swap
        Range *sliceRange = o.getSpan();
        Value *left       = o.getSpan()->getLeftBound();
        Value *right      = o.getSpan()->getRightBound();
        sliceRange->setLeftBound(right);
        sliceRange->setRightBound(left);
        sliceRange->setDirection(prefix_dir);
    }

    return 0;
}

auto PostParsingVisitor_step2::visitStateTable(StateTable &o) -> int
{
    GuideVisitor::visitStateTable(o);

    if (!o.checkProperty(HIF_CONCURRENT_ASSERTION)) {
        return 0;
    }
    o.removeProperty(HIF_CONCURRENT_ASSERTION);

    auto *assertion = dynamic_cast<ProcedureCall *>(o.states.front()->actions.front());
    messageAssert(assertion != nullptr, "Expected assertion", o.states.front()->actions.front(), _sem);

    hif::semantics::SymbolList list;
    hif::semantics::collectSymbols(list, assertion->parameterAssigns.front()->getValue(), _sem);
    hif::manipulation::AddUniqueObjectOptions opt;
    opt.copyIfUnique = true;
    for (auto *symbol : list) {
        auto *libInst = dynamic_cast<Instance *>(symbol);
        if (libInst != nullptr) {
            continue;
        }

        Declaration *decl = hif::semantics::getDeclaration(symbol, _sem);
        messageAssert(decl != nullptr, "Declaration not found", symbol, _sem);

        Port *pdecl = dynamic_cast<Port *>(decl);
        auto *sdecl = dynamic_cast<Signal *>(decl);

        if (pdecl == nullptr && sdecl == nullptr) {
            continue;
        }

        auto *current  = dynamic_cast<Value *>(symbol);
        Object *parent = current->getParent();
        while (parent != nullptr) {
            auto *slice  = dynamic_cast<Slice *>(parent);
            auto *member = dynamic_cast<Member *>(parent);
            if (member != nullptr && member->getPrefix() != current) {
                member = nullptr;
            }
            if (slice != nullptr) {
                current = slice;
            } else if (member != nullptr) {
                current = member;
            } else {
                break;
            }
            parent = current->getParent();
        }

        hif::manipulation::addUniqueObject(current, o.sensitivity, opt);
    }

    return 0;
}

auto PostParsingVisitor_step2::visitSwitch(hif::Switch &o) -> int
{
    GuideVisitor::visitSwitch(o);

    Type *st = hif::semantics::getSemanticType(o.getCondition(), _sem);
    messageAssert(st != nullptr, "Cannot type switch value", o.getCondition(), _sem);

    Char *cc = dynamic_cast<Char *>(st);
    if (cc == nullptr) {
        return 0;
    }

    // Maybe chars has been parser as bitval
    // Fixing constants into cases.
    for (BList<SwitchAlt>::iterator i = o.alts.begin(); i != o.alts.end(); ++i) {
        SwitchAlt *swa = *i;
        for (BList<Value>::iterator j = swa->conditions.begin(); j != swa->conditions.end(); ++j) {
            if (dynamic_cast<Range *>(*j) != nullptr) {
                auto *r = dynamic_cast<Range *>(*j);

                Type *rb = hif::semantics::getSemanticType(r->getRightBound(), _sem);
                messageAssert(rb != nullptr, "Cannot type rbound", r->getRightBound(), _sem);

                Type *lb = hif::semantics::getSemanticType(r->getLeftBound(), _sem);
                messageAssert(lb != nullptr, "Cannot type lbound", r->getLeftBound(), _sem);

                if (dynamic_cast<Char *>(rb) == nullptr) {
                    Cast *c = new Cast();
                    c->setValue(r->setRightBound(nullptr));
                    c->setType(hif::copy(st));
                    r->setRightBound(c);
                }

                if (dynamic_cast<Char *>(lb) == nullptr) {
                    Cast *c = new Cast();
                    c->setValue(r->setLeftBound(nullptr));
                    c->setType(hif::copy(st));
                    r->setLeftBound(c);
                }

                continue;
            }

            // value case
            Type *vt = hif::semantics::getSemanticType(*j, _sem);
            messageAssert(vt != nullptr, "Cannot type value", *j, _sem);

            if (dynamic_cast<Char *>(vt) == nullptr) {
                Cast *c = new Cast();
                c->setType(hif::copy(st));
                Value *v = (*j);
                v->replace(c);
                c->setValue(v);
            }
        }
    }

    return 0;
}

auto PostParsingVisitor_step2::visitSystem(System &o) -> int
{
    for (BList<LibraryDef>::iterator i = o.libraryDefs.begin(); i != o.libraryDefs.end(); ++i) {
        LibraryDef *ld = *i;
        _addStandardOperatorOverloads(ld);
    }

    GuideVisitor::visitSystem(o);

    return 0;
}

void PostParsingVisitor_step2::_manageForDeclarations(For &o)
{
    // List of init declarations or init values must be empty.
    messageDebugAssert(o.initDeclarations.empty() || o.initValues.empty(), "Unexpected for", &o, _sem);

    // Init declarations are already ok.
    // All init values must have a correspondent declaration. Otherwise, create it.
    for (BList<Action>::iterator it = o.initValues.begin(); it != o.initValues.end(); ++it) {
        auto *as = dynamic_cast<Assign *>(*it);
        // TODO: check other cases!
        messageAssert(as != nullptr, "Unexpected for init value", *it, _sem);

        auto *id = dynamic_cast<Identifier *>(as->getLeftHandSide());
        messageAssert(id != nullptr, "Unexpected for init value assign", as, _sem);

        hif::semantics::DeclarationOptions dopt;
        dopt.location = &o;
        auto *ddo     = dynamic_cast<Variable *>(hif::semantics::getDeclaration(as->getLeftHandSide(), _sem, dopt));
        if (ddo != nullptr) {
            continue;
        }

        auto *sto = hif::getNearestParent<StateTable>(&o);
        messageAssert(sto != nullptr, "Cannot find nearest state table", &o, _sem);

        // Add a fresh variable declaration of for index and replace all occurences
        // of old name in branch under for node.
        Variable *vo = _factory.variable(_factory.integer(nullptr, true, false), id->getName(), _factory.intval(0));
        sto->declarations.push_back(vo);
    }
}

auto PostParsingVisitor_step2::_manageLoopIndex(Value *condition) -> Value *
{
    auto *forRange = dynamic_cast<Range *>(condition);
    messageAssert(forRange != nullptr, "Unexpected for/forGenerate condition.", condition, _sem);

    Value *v = hif::copy(forRange->getLeftBound());
    hif::manipulation::assureSyntacticType(v, _sem);
    return v;
}

template <typename T> void PostParsingVisitor_step2::_manageForIndex(T &o)
{
    Value *indexInitVal = _manageLoopIndex(o.getCondition());

    // set the init val
    messageAssert(!o.initDeclarations.empty(), "Unexpected empty initial declaration", &o, _sem);
    delete o.initDeclarations.front()->setValue(indexInitVal);

    hif::manipulation::simplify(&o, _sem);
}

auto PostParsingVisitor_step2::_transformRangeToExpression(BList<DataDeclaration> &initDeclarations, Value *condition)
    -> Value *
{
    hif::HifFactory f;
    f.setSemantics(_sem);
    messageAssert(initDeclarations.size() == 1, "Unexpected case", nullptr, _sem);

    DataDeclaration *indDecl = initDeclarations.back();
    auto *forRange           = dynamic_cast<Range *>(condition);
    messageAssert(forRange != nullptr, "Unexpected for condition", condition, _sem);

    Operator condOp = op_none;
    Value *loopEnd  = nullptr;
    if (forRange->getDirection() == dir_downto) {
        loopEnd = hif::copy(forRange->getRightBound());
        condOp  = op_ge;
    } else if (forRange->getDirection() == dir_upto) {
        loopEnd = hif::copy(forRange->getRightBound());
        condOp  = op_le;
    } else {
        messageError("Unexpected direction", forRange, _sem);
    }

    if (dynamic_cast<ConstValue *>(loopEnd) != nullptr) {
        auto *cv = dynamic_cast<ConstValue *>(loopEnd);
        if (cv->getType() == nullptr) {
            cv->setType(_sem->getTypeForConstant(cv));
        }
    }

    // TODO check: if forRange->getRightBound() contains references to ind, shall we change
    // these with references to ind_hif_support
    auto *ind = new Identifier(indDecl->getName());

    return new Expression(condOp, ind, f.cast(hif::copy(indDecl->getType()), loopEnd));
}

void PostParsingVisitor_step2::_manageForRange(hif::For &o)
{
    Value *newCond = _transformRangeToExpression(o.initDeclarations, o.getCondition());
    delete o.setCondition(newCond);
}

void PostParsingVisitor_step2::_manageForGenerateRange(hif::ForGenerate &o)
{
    Value *newCond = _transformRangeToExpression(o.initDeclarations, o.getCondition());
    delete o.setCondition(newCond);
}

auto PostParsingVisitor_step2::_fixOverloadedOperators(Expression &o) -> bool
{
    // Establish operator's type basing on VHDL semantics
    hif::semantics::getSemanticType(o.getValue1(), _sem);

    if (o.getValue2() != nullptr) {
        hif::semantics::getSemanticType(o.getValue2(), _sem);
        if (!_isOverloadable(o.getOperator(), o.getValue1()->getSemanticType(), o.getValue2()->getSemanticType())) {
            return false;
        }
    } else {
        if (!_isOverloadable(o.getOperator(), o.getValue1()->getSemanticType(), nullptr)) {
            return false;
        }
    }

    // Fix overloaded operators
    // expression may be derived from an overloading of operator.

    // get the overloaded operator name.
    std::string fname = _getOverloadedFunctionName(o.getOperator());
    if (fname.empty()) {
        // if not present, try to type expression normally.
        Type *type = hif::semantics::getSemanticType(&o, _sem);
        if (type == nullptr) {
            messageError("Not able to calculate type of expression", &o, _sem);
        }
        return false;
    }

    // overloaded operator present, build a call to this function.
    auto *fcall = new FunctionCall();
    fcall->setName(fname);

    // set parameters
    auto *pop1 = new ParameterAssign();
    pop1->setValue(hif::copy(o.getValue1()));
    fcall->parameterAssigns.push_back(pop1);
    if (o.getValue2() != nullptr) {
        auto *pop2 = new ParameterAssign();
        pop2->setValue(hif::copy(o.getValue2()));
        fcall->parameterAssigns.push_back(pop2);
    }

    // try to get declaration of created function
    std::list<FunctionCall::DeclarationType *> list;
    hif::semantics::GetCandidatesOptions opt;
    opt.getAllAssignables = true;
    opt.location          = &o;
    hif::semantics::getCandidates(list, fcall, _sem, opt);
    if (!list.empty()) {
        hif::semantics::DeclarationOptions dopt;
        dopt.location  = &o;
        Function *func = hif::semantics::getDeclaration(fcall, _sem, dopt);
        messageAssert(func != nullptr, "Declaration not found", fcall, _sem);

        // Check inside set since there are some overloaded operators inside
        // VHDL Semantics library defs that must be propagated to HIF.
        if (_operators.find(func) != _operators.end()) {
            delete fcall;
            Type *type = hif::semantics::getSemanticType(&o, _sem);
            if (type == nullptr) {
                messageError("Not able to calculate type of expression", &o, _sem);
            }
            return false;
        }

        // found, replace expression with function call to overloaded operator.
        _factory.codeInfo(fcall, o.getCodeInfo());
        o.replace(fcall);
        delete &o;

        BList<Parameter>::iterator j       = func->parameters.begin();
        BList<ParameterAssign>::iterator i = fcall->parameterAssigns.begin();
        for (; i != fcall->parameterAssigns.end(); ++i, ++j) {
            ParameterAssign *pao_fcall = *i;
            auto *pao_decl             = dynamic_cast<DataDeclaration *>((*j));

            if (pao_fcall->getName() == NameTable::getInstance()->none()) {
                pao_fcall->setName(pao_decl->getName());
            }
        }
        return true;
    } else {
        delete fcall;
        // not found, probably it was not a function call to overloaded operator.
        // Try to type original expression normally.
        Type *type = hif::semantics::getSemanticType(&o, _sem);
        if (type == nullptr) {
            messageError("Not able to calculate type of expression", &o, _sem);
        }
    }

    return false;
}

auto PostParsingVisitor_step2::_isOverloadable(const Operator /*op*/, Type *t1, Type *t2) -> bool
{
    EqualsOptions opt;
    opt.checkOnlyTypes = true;
    return (t2 != nullptr && !hif::equals(t1, t2, opt)) ||
           (dynamic_cast<Int *>(t1) == nullptr && dynamic_cast<Bool *>(t1) == nullptr &&
            dynamic_cast<Time *>(t1) == nullptr &&
            (dynamic_cast<Bit *>(t1) == nullptr || dynamic_cast<Bit *>(t1)->isLogic()) &&
            (dynamic_cast<Bitvector *>(t1) == nullptr || dynamic_cast<Bitvector *>(t1)->isLogic()));
}

void PostParsingVisitor_step2::_addStandardOperatorOverloads(LibraryDef *o)
{
    if (!o->isStandard()) {
        return;
    }

    if (o->getName() == "ieee_numeric_std") {
        //_parseOperator(o, "abs", "signed", "signed");
        _parseOperator(o, "-", "signed", "signed");
        _parseOperator(o, "+", "unsigned", "unsigned", "unsigned");
        _parseOperator(o, "+", "signed", "signed", "signed");
        _parseOperator(o, "+", "unsigned", "natural", "unsigned");
        _parseOperator(o, "+", "natural", "unsigned", "unsigned");
        _parseOperator(o, "+", "integer", "signed", "signed");
        _parseOperator(o, "+", "signed", "integer", "signed");
        _parseOperator(o, "-", "unsigned", "unsigned", "unsigned");
        _parseOperator(o, "-", "signed", "signed", "signed");
        _parseOperator(o, "-", "unsigned", "natural", "unsigned");
        _parseOperator(o, "-", "natural", "unsigned", "unsigned");
        _parseOperator(o, "-", "signed", "integer", "signed");
        _parseOperator(o, "-", "integer", "signed", "signed");
        _parseOperator(o, "*", "unsigned", "unsigned", "unsigned");
        _parseOperator(o, "*", "signed", "signed", "signed");
        _parseOperator(o, "*", "unsigned", "natural", "unsigned");
        _parseOperator(o, "*", "natural", "unsigned", "unsigned");
        _parseOperator(o, "*", "signed", "integer", "signed");
        _parseOperator(o, "*", "integer", "signed", "signed");
        _parseOperator(o, "/", "unsigned", "unsigned", "unsigned");
        _parseOperator(o, "/", "signed", "signed", "signed");
        _parseOperator(o, "/", "unsigned", "natural", "unsigned");
        _parseOperator(o, "/", "natural", "unsigned", "unsigned");
        _parseOperator(o, "/", "signed", "integer", "signed");
        _parseOperator(o, "/", "integer", "signed", "signed");
        _parseOperator(o, "rem", "unsigned", "unsigned", "unsigned");
        _parseOperator(o, "rem", "signed", "signed", "signed");
        _parseOperator(o, "rem", "unsigned", "natural", "unsigned");
        _parseOperator(o, "rem", "natural", "unsigned", "unsigned");
        _parseOperator(o, "rem", "signed", "integer", "signed");
        _parseOperator(o, "rem", "integer", "signed", "signed");
        _parseOperator(o, "mod", "unsigned", "unsigned", "unsigned");
        _parseOperator(o, "mod", "signed", "signed", "signed");
        _parseOperator(o, "mod", "unsigned", "natural", "unsigned");
        _parseOperator(o, "mod", "natural", "unsigned", "unsigned");
        _parseOperator(o, "mod", "signed", "integer", "signed");
        _parseOperator(o, "mod", "integer", "signed", "signed");
        _parseOperator(o, ">", "unsigned", "unsigned", "boolean");
        _parseOperator(o, ">", "signed", "signed", "boolean");
        _parseOperator(o, ">", "natural", "unsigned", "boolean");
        _parseOperator(o, ">", "integer", "signed", "boolean");
        _parseOperator(o, ">", "unsigned", "natural", "boolean");
        _parseOperator(o, ">", "signed", "integer", "boolean");
        _parseOperator(o, "<", "unsigned", "unsigned", "boolean");
        _parseOperator(o, "<", "signed", "signed", "boolean");
        _parseOperator(o, "<", "natural", "unsigned", "boolean");
        _parseOperator(o, "<", "integer", "signed", "boolean");
        _parseOperator(o, "<", "unsigned", "natural", "boolean");
        _parseOperator(o, "<", "signed", "integer", "boolean");
        _parseOperator(o, "<=", "unsigned", "unsigned", "boolean");
        _parseOperator(o, "<=", "signed", "signed", "boolean");
        _parseOperator(o, "<=", "natural", "unsigned", "boolean");
        _parseOperator(o, "<=", "integer", "signed", "boolean");
        _parseOperator(o, "<=", "unsigned", "natural", "boolean");
        _parseOperator(o, "<=", "signed", "integer", "boolean");
        _parseOperator(o, ">=", "unsigned", "unsigned", "boolean");
        _parseOperator(o, ">=", "signed", "signed", "boolean");
        _parseOperator(o, ">=", "natural", "unsigned", "boolean");
        _parseOperator(o, ">=", "integer", "signed", "boolean");
        _parseOperator(o, ">=", "unsigned", "natural", "boolean");
        _parseOperator(o, ">=", "signed", "integer", "boolean");
        _parseOperator(o, "=", "unsigned", "unsigned", "boolean");
        _parseOperator(o, "=", "signed", "signed", "boolean");
        _parseOperator(o, "=", "natural", "unsigned", "boolean");
        _parseOperator(o, "=", "integer", "signed", "boolean");
        _parseOperator(o, "=", "unsigned", "natural", "boolean");
        _parseOperator(o, "=", "signed", "integer", "boolean");
        _parseOperator(o, "/=", "unsigned", "unsigned", "boolean");
        _parseOperator(o, "/=", "signed", "signed", "boolean");
        _parseOperator(o, "/=", "natural", "unsigned", "boolean");
        _parseOperator(o, "/=", "integer", "signed", "boolean");
        _parseOperator(o, "/=", "unsigned", "natural", "boolean");
        _parseOperator(o, "/=", "signed", "integer", "boolean");
        _parseOperator(o, "not", "unsigned", "unsigned");
        _parseOperator(o, "and", "unsigned", "unsigned", "unsigned");
        _parseOperator(o, "or", "unsigned", "unsigned", "unsigned");
        _parseOperator(o, "nand", "unsigned", "unsigned", "unsigned");
        _parseOperator(o, "nor", "unsigned", "unsigned", "unsigned");
        _parseOperator(o, "xor", "unsigned", "unsigned", "unsigned");
        _parseOperator(o, "not", "signed", "signed");
        _parseOperator(o, "and", "signed", "signed", "signed");
        _parseOperator(o, "or", "signed", "signed", "signed");
        _parseOperator(o, "nand", "signed", "signed", "signed");
        _parseOperator(o, "nor", "signed", "signed", "signed");
        _parseOperator(o, "xor", "signed", "signed", "signed");
    } else if (o->getName() == "ieee_std_logic_arith") {
        _parseOperator(o, "+", "unsigned", "unsigned", "unsigned");
        _parseOperator(o, "+", "signed", "signed", "signed");
        _parseOperator(o, "+", "unsigned", "signed", "signed");
        _parseOperator(o, "+", "signed", "unsigned", "signed");
        _parseOperator(o, "+", "unsigned", "integer", "unsigned");
        _parseOperator(o, "+", "integer", "unsigned", "unsigned");
        _parseOperator(o, "+", "signed", "integer", "signed");
        _parseOperator(o, "+", "integer", "signed", "signed");
        _parseOperator(o, "+", "unsigned", "std_ulogic", "unsigned");
        _parseOperator(o, "+", "std_ulogic", "unsigned", "unsigned");
        _parseOperator(o, "+", "signed", "std_ulogic", "signed");
        _parseOperator(o, "+", "std_ulogic", "signed", "signed");
        //_parseOperator(o, "+", "unsigned", "unsigned", "std_logic_vector");
        //_parseOperator(o, "+", "signed", "signed", "std_logic_vector");
        //_parseOperator(o, "+", "unsigned", "signed", "std_logic_vector");
        //_parseOperator(o, "+", "signed", "unsigned", "std_logic_vector");
        //_parseOperator(o, "+", "unsigned", "integer", "std_logic_vector");
        //_parseOperator(o, "+", "integer", "unsigned", "std_logic_vector");
        //_parseOperator(o, "+", "signed", "integer", "std_logic_vector");
        //_parseOperator(o, "+", "integer", "signed", "std_logic_vector");
        //_parseOperator(o, "+", "unsigned", "std_ulogic", "std_logic_vector");
        //_parseOperator(o, "+", "std_ulogic", "unsigned", "std_logic_vector");
        //_parseOperator(o, "+", "signed", "std_ulogic", "std_logic_vector");
        //_parseOperator(o, "+", "std_ulogic", "signed", "std_logic_vector");
        _parseOperator(o, "-", "unsigned", "unsigned", "unsigned");
        _parseOperator(o, "-", "signed", "signed", "signed");
        _parseOperator(o, "-", "unsigned", "signed", "signed");
        _parseOperator(o, "-", "signed", "unsigned", "signed");
        _parseOperator(o, "-", "unsigned", "integer", "unsigned");
        _parseOperator(o, "-", "integer", "unsigned", "unsigned");
        _parseOperator(o, "-", "signed", "integer", "signed");
        _parseOperator(o, "-", "integer", "signed", "signed");
        _parseOperator(o, "-", "unsigned", "std_ulogic", "unsigned");
        _parseOperator(o, "-", "std_ulogic", "unsigned", "unsigned");
        _parseOperator(o, "-", "signed", "std_ulogic", "signed");
        _parseOperator(o, "-", "std_ulogic", "signed", "signed");
        //_parseOperator(o, "-", "unsigned", "unsigned", "std_logic_vector");
        //_parseOperator(o, "-", "signed", "signed", "std_logic_vector");
        //_parseOperator(o, "-", "unsigned", "signed", "std_logic_vector");
        //_parseOperator(o, "-", "signed", "unsigned", "std_logic_vector");
        //_parseOperator(o, "-", "unsigned", "integer", "std_logic_vector");
        //_parseOperator(o, "-", "integer", "unsigned", "std_logic_vector");
        //_parseOperator(o, "-", "signed", "integer", "std_logic_vector");
        //_parseOperator(o, "-", "integer", "signed", "std_logic_vector");
        //_parseOperator(o, "-", "unsigned", "std_ulogic", "std_logic_vector");
        //_parseOperator(o, "-", "std_ulogic", "unsigned", "std_logic_vector");
        //_parseOperator(o, "-", "signed", "std_ulogic", "std_logic_vector");
        //_parseOperator(o, "-", "std_ulogic", "signed", "std_logic_vector");
        _parseOperator(o, "+", "unsigned", "unsigned");
        _parseOperator(o, "+", "signed", "signed");
        _parseOperator(o, "-", "signed", "signed");
        //_parseOperator(o, "abs", "signed", "signed");
        //_parseOperator(o, "+", "unsigned", "std_logic_vector");
        //_parseOperator(o, "+", "signed", "std_logic_vector");
        //_parseOperator(o, "-", "signed", "std_logic_vector");
        //_parseOperator(o, "abs", "signed", "std_logic_vector");
        _parseOperator(o, "*", "unsigned", "unsigned", "unsigned");
        _parseOperator(o, "*", "signed", "signed", "signed");
        _parseOperator(o, "*", "signed", "unsigned", "signed");
        _parseOperator(o, "*", "unsigned", "signed", "signed");
        //_parseOperator(o, "*", "unsigned", "unsigned", "std_logic_vector");
        //_parseOperator(o, "*", "signed", "signed", "std_logic_vector");
        //_parseOperator(o, "*", "signed", "unsigned", "std_logic_vector");
        //_parseOperator(o, "*", "unsigned", "signed", "std_logic_vector");
        _parseOperator(o, "<", "unsigned", "unsigned", "boolean");
        _parseOperator(o, "<", "signed", "signed", "boolean");
        _parseOperator(o, "<", "unsigned", "signed", "boolean");
        _parseOperator(o, "<", "signed", "unsigned", "boolean");
        _parseOperator(o, "<", "unsigned", "integer", "boolean");
        _parseOperator(o, "<", "integer", "unsigned", "boolean");
        _parseOperator(o, "<", "signed", "integer", "boolean");
        _parseOperator(o, "<", "integer", "signed", "boolean");
        _parseOperator(o, "<=", "unsigned", "unsigned", "boolean");
        _parseOperator(o, "<=", "signed", "signed", "boolean");
        _parseOperator(o, "<=", "unsigned", "signed", "boolean");
        _parseOperator(o, "<=", "signed", "unsigned", "boolean");
        _parseOperator(o, "<=", "unsigned", "integer", "boolean");
        _parseOperator(o, "<=", "integer", "unsigned", "boolean");
        _parseOperator(o, "<=", "signed", "integer", "boolean");
        _parseOperator(o, "<=", "integer", "signed", "boolean");
        _parseOperator(o, ">", "unsigned", "unsigned", "boolean");
        _parseOperator(o, ">", "signed", "signed", "boolean");
        _parseOperator(o, ">", "unsigned", "signed", "boolean");
        _parseOperator(o, ">", "signed", "unsigned", "boolean");
        _parseOperator(o, ">", "unsigned", "integer", "boolean");
        _parseOperator(o, ">", "integer", "unsigned", "boolean");
        _parseOperator(o, ">", "signed", "integer", "boolean");
        _parseOperator(o, ">", "integer", "signed", "boolean");
        _parseOperator(o, ">=", "unsigned", "unsigned", "boolean");
        _parseOperator(o, ">=", "signed", "signed", "boolean");
        _parseOperator(o, ">=", "unsigned", "signed", "boolean");
        _parseOperator(o, ">=", "signed", "unsigned", "boolean");
        _parseOperator(o, ">=", "unsigned", "integer", "boolean");
        _parseOperator(o, ">=", "integer", "unsigned", "boolean");
        _parseOperator(o, ">=", "signed", "integer", "boolean");
        _parseOperator(o, ">=", "integer", "signed", "boolean");
        _parseOperator(o, "=", "unsigned", "unsigned", "boolean");
        _parseOperator(o, "=", "signed", "signed", "boolean");
        _parseOperator(o, "=", "unsigned", "signed", "boolean");
        _parseOperator(o, "=", "signed", "unsigned", "boolean");
        _parseOperator(o, "=", "unsigned", "integer", "boolean");
        _parseOperator(o, "=", "integer", "unsigned", "boolean");
        _parseOperator(o, "=", "signed", "integer", "boolean");
        _parseOperator(o, "=", "integer", "signed", "boolean");
        _parseOperator(o, "/=", "unsigned", "unsigned", "boolean");
        _parseOperator(o, "/=", "signed", "signed", "boolean");
        _parseOperator(o, "/=", "unsigned", "signed", "boolean");
        _parseOperator(o, "/=", "signed", "unsigned", "boolean");
        _parseOperator(o, "/=", "unsigned", "integer", "boolean");
        _parseOperator(o, "/=", "integer", "unsigned", "boolean");
        _parseOperator(o, "/=", "signed", "integer", "boolean");
        _parseOperator(o, "/=", "integer", "signed", "boolean");
    } else if (o->getName() == "ieee_std_logic_1164") {
        _parseOperator(o, "and", "std_ulogic", "std_ulogic", "ux01");
        _parseOperator(o, "nand", "std_ulogic", "std_ulogic", "ux01");
        _parseOperator(o, "or", "std_ulogic", "std_ulogic", "ux01");
        _parseOperator(o, "nor", "std_ulogic", "std_ulogic", "ux01");
        _parseOperator(o, "xor", "std_ulogic", "std_ulogic", "ux01");
        _parseOperator(o, "xnor", "std_ulogic", "std_ulogic", "ux01");
        _parseOperator(o, "not", "std_ulogic", "ux01");
        _parseOperator(o, "and", "std_logic_vector", "std_logic_vector", "std_logic_vector");
        _parseOperator(o, "and", "std_ulogic_vector", "std_ulogic_vector", "std_ulogic_vector");
        _parseOperator(o, "nand", "std_logic_vector", "std_logic_vector", "std_logic_vector");
        _parseOperator(o, "nand", "std_ulogic_vector", "std_ulogic_vector", "std_ulogic_vector");
        _parseOperator(o, "or", "std_logic_vector", "std_logic_vector", "std_logic_vector");
        _parseOperator(o, "or", "std_ulogic_vector", "std_ulogic_vector", "std_ulogic_vector");
        _parseOperator(o, "nor", "std_logic_vector", "std_logic_vector", "std_logic_vector");
        _parseOperator(o, "nor", "std_ulogic_vector", "std_ulogic_vector", "std_ulogic_vector");
        _parseOperator(o, "xor", "std_logic_vector", "std_logic_vector", "std_logic_vector");
        _parseOperator(o, "xor", "std_ulogic_vector", "std_ulogic_vector", "std_ulogic_vector");
        _parseOperator(o, "xnor", "std_logic_vector", "std_logic_vector", "std_logic_vector");
        _parseOperator(o, "xnor", "std_ulogic_vector", "std_ulogic_vector", "std_ulogic_vector");
        _parseOperator(o, "not", "std_logic_vector", "std_logic_vector");
        _parseOperator(o, "not", "std_ulogic_vector", "std_ulogic_vector");
    } else if (o->getName() == "ieee_std_logic_arith_ex") {
        _parseOperator(o, "+", "std_logic_vector", "std_logic_vector", "std_logic_vector");
        _parseOperator(o, "+", "std_logic_vector", "std_ulogic", "std_logic_vector");
        _parseOperator(o, "+", "std_ulogic", "std_logic_vector", "std_logic_vector");
        _parseOperator(o, "+", "integer", "std_logic_vector", "std_logic_vector");
        _parseOperator(o, "+", "std_logic_vector", "integer", "std_logic_vector");
        _parseOperator(o, "+", "std_ulogic_vector", "std_ulogic_vector", "std_ulogic_vector");
        _parseOperator(o, "+", "std_ulogic_vector", "std_ulogic", "std_ulogic_vector");
        _parseOperator(o, "+", "std_ulogic", "std_ulogic_vector", "std_ulogic_vector");
        _parseOperator(o, "+", "integer", "std_ulogic_vector", "std_ulogic_vector");
        _parseOperator(o, "+", "std_ulogic_vector", "integer", "std_ulogic_vector");
        _parseOperator(o, "+", "std_ulogic", "std_ulogic", "std_ulogic");
        _parseOperator(o, "-", "std_logic_vector", "std_logic_vector", "std_logic_vector");
        _parseOperator(o, "-", "std_logic_vector", "std_ulogic", "std_logic_vector");
        _parseOperator(o, "-", "std_ulogic", "std_logic_vector", "std_logic_vector");
        _parseOperator(o, "-", "integer", "std_logic_vector", "std_logic_vector");
        _parseOperator(o, "-", "std_logic_vector", "integer", "std_logic_vector");
        _parseOperator(o, "-", "std_ulogic_vector", "std_ulogic_vector", "std_ulogic_vector");
        _parseOperator(o, "-", "std_ulogic_vector", "std_ulogic", "std_ulogic_vector");
        _parseOperator(o, "-", "std_ulogic", "std_ulogic_vector", "std_ulogic_vector");
        _parseOperator(o, "-", "integer", "std_ulogic_vector", "std_ulogic_vector");
        _parseOperator(o, "-", "std_ulogic_vector", "integer", "std_ulogic_vector");
        _parseOperator(o, "-", "std_ulogic", "std_ulogic", "std_ulogic");
        _parseOperator(o, "+", "std_logic_vector", "std_logic_vector");
        _parseOperator(o, "+", "std_ulogic_vector", "std_ulogic_vector");
        _parseOperator(o, "*", "std_logic_vector", "std_logic_vector", "std_logic_vector");
        _parseOperator(o, "*", "std_ulogic", "std_logic_vector", "std_logic_vector");
        _parseOperator(o, "*", "std_logic_vector", "std_ulogic", "std_logic_vector");
        _parseOperator(o, "*", "std_ulogic", "std_ulogic", "std_ulogic");
        _parseOperator(o, "*", "std_ulogic_vector", "std_ulogic_vector", "std_ulogic_vector");
        _parseOperator(o, "*", "std_ulogic", "std_ulogic_vector", "std_ulogic_vector");
        _parseOperator(o, "*", "std_ulogic_vector", "std_ulogic", "std_ulogic_vector");
        _parseOperator(o, "/", "std_logic_vector", "std_logic_vector", "std_logic_vector");
        _parseOperator(o, "/", "std_logic_vector", "std_ulogic", "std_logic_vector");
        _parseOperator(o, "/", "std_ulogic", "std_logic_vector", "std_ulogic");
        _parseOperator(o, "/", "std_ulogic", "std_ulogic", "std_ulogic");
        _parseOperator(o, "<", "integer", "std_logic_vector", "boolean");
        _parseOperator(o, "<", "std_logic_vector", "integer", "boolean");
        _parseOperator(o, "<=", "integer", "std_logic_vector", "boolean");
        _parseOperator(o, "<=", "std_logic_vector", "integer", "boolean");
        _parseOperator(o, ">", "integer", "std_logic_vector", "boolean");
        _parseOperator(o, ">", "std_logic_vector", "integer", "boolean");
        _parseOperator(o, ">=", "integer", "std_logic_vector", "boolean");
        _parseOperator(o, ">=", "std_logic_vector", "integer", "boolean");
        _parseOperator(o, "=", "integer", "std_logic_vector", "boolean");
        _parseOperator(o, "=", "std_logic_vector", "integer", "boolean");
        _parseOperator(o, "/=", "integer", "std_logic_vector", "boolean");
        _parseOperator(o, "/=", "std_logic_vector", "integer", "boolean");
        _parseOperator(o, "<", "integer", "std_ulogic_vector", "boolean");
        _parseOperator(o, "<", "std_ulogic_vector", "integer", "boolean");
        _parseOperator(o, "<=", "integer", "std_ulogic_vector", "boolean");
        _parseOperator(o, "<=", "std_ulogic_vector", "integer", "boolean");
        _parseOperator(o, ">", "integer", "std_ulogic_vector", "boolean");
        _parseOperator(o, ">", "std_ulogic_vector", "integer", "boolean");
        _parseOperator(o, ">=", "integer", "std_ulogic_vector", "boolean");
        _parseOperator(o, ">=", "std_ulogic_vector", "integer", "boolean");
        _parseOperator(o, "=", "integer", "std_ulogic_vector", "boolean");
        _parseOperator(o, "=", "std_ulogic_vector", "integer", "boolean");
        _parseOperator(o, "/=", "integer", "std_ulogic_vector", "boolean");
        _parseOperator(o, "/=", "std_ulogic_vector", "integer", "boolean");
    } else if (o->getName() == "ieee_numeric_bit") {
        _parseOperator(o, "abs", "signed", "signed");
        _parseOperator(o, "-", "signed", "signed");
        _parseOperator(o, "+", "unsigned", "unsigned", "unsigned");
        _parseOperator(o, "+", "signed", "signed", "signed");
        _parseOperator(o, "+", "unsigned", "natural", "unsigned");
        _parseOperator(o, "+", "natural", "unsigned", "unsigned");
        _parseOperator(o, "+", "integer", "signed", "signed");
        _parseOperator(o, "+", "signed", "integer", "signed");
        _parseOperator(o, "-", "unsigned", "unsigned", "unsigned");
        _parseOperator(o, "-", "signed", "signed", "signed");
        _parseOperator(o, "-", "unsigned", "natural", "unsigned");
        _parseOperator(o, "-", "natural", "unsigned", "unsigned");
        _parseOperator(o, "-", "signed", "integer", "signed");
        _parseOperator(o, "-", "integer", "signed", "signed");
        _parseOperator(o, "*", "unsigned", "unsigned", "unsigned");
        _parseOperator(o, "*", "signed", "signed", "signed");
        _parseOperator(o, "*", "unsigned", "natural", "unsigned");
        _parseOperator(o, "*", "natural", "unsigned", "unsigned");
        _parseOperator(o, "*", "signed", "integer", "signed");
        _parseOperator(o, "*", "integer", "signed", "signed");
        _parseOperator(o, "/", "unsigned", "unsigned", "unsigned");
        _parseOperator(o, "/", "signed", "signed", "signed");
        _parseOperator(o, "/", "unsigned", "natural", "unsigned");
        _parseOperator(o, "/", "natural", "unsigned", "unsigned");
        _parseOperator(o, "/", "signed", "integer", "signed");
        _parseOperator(o, "/", "integer", "signed", "signed");
        _parseOperator(o, "rem", "unsigned", "unsigned", "unsigned");
        _parseOperator(o, "rem", "signed", "signed", "signed");
        _parseOperator(o, "rem", "unsigned", "natural", "unsigned");
        _parseOperator(o, "rem", "natural", "unsigned", "unsigned");
        _parseOperator(o, "rem", "signed", "integer", "signed");
        _parseOperator(o, "rem", "integer", "signed", "signed");
        _parseOperator(o, "mod", "unsigned", "unsigned", "unsigned");
        _parseOperator(o, "mod", "signed", "signed", "signed");
        _parseOperator(o, "mod", "unsigned", "natural", "unsigned");
        _parseOperator(o, "mod", "natural", "unsigned", "unsigned");
        _parseOperator(o, "mod", "signed", "integer", "signed");
        _parseOperator(o, "mod", "integer", "signed", "signed");
        _parseOperator(o, ">", "unsigned", "unsigned", "boolean");
        _parseOperator(o, ">", "signed", "signed", "boolean");
        _parseOperator(o, ">", "natural", "unsigned", "boolean");
        _parseOperator(o, ">", "integer", "signed", "boolean");
        _parseOperator(o, ">", "unsigned", "natural", "boolean");
        _parseOperator(o, ">", "signed", "integer", "boolean");
        _parseOperator(o, "<", "unsigned", "unsigned", "boolean");
        _parseOperator(o, "<", "signed", "signed", "boolean");
        _parseOperator(o, "<", "natural", "unsigned", "boolean");
        _parseOperator(o, "<", "integer", "signed", "boolean");
        _parseOperator(o, "<", "unsigned", "natural", "boolean");
        _parseOperator(o, "<", "signed", "integer", "boolean");
        _parseOperator(o, "<=", "unsigned", "unsigned", "boolean");
        _parseOperator(o, "<=", "signed", "signed", "boolean");
        _parseOperator(o, "<=", "natural", "unsigned", "boolean");
        _parseOperator(o, "<=", "integer", "signed", "boolean");
        _parseOperator(o, "<=", "unsigned", "natural", "boolean");
        _parseOperator(o, "<=", "signed", "integer", "boolean");
        _parseOperator(o, ">=", "unsigned", "unsigned", "boolean");
        _parseOperator(o, ">=", "signed", "signed", "boolean");
        _parseOperator(o, ">=", "natural", "unsigned", "boolean");
        _parseOperator(o, ">=", "integer", "signed", "boolean");
        _parseOperator(o, ">=", "unsigned", "natural", "boolean");
        _parseOperator(o, ">=", "signed", "integer", "boolean");
        _parseOperator(o, "=", "unsigned", "unsigned", "boolean");
        _parseOperator(o, "=", "signed", "signed", "boolean");
        _parseOperator(o, "=", "natural", "unsigned", "boolean");
        _parseOperator(o, "=", "integer", "signed", "boolean");
        _parseOperator(o, "=", "unsigned", "natural", "boolean");
        _parseOperator(o, "=", "signed", "integer", "boolean");
        _parseOperator(o, "/=", "unsigned", "unsigned", "boolean");
        _parseOperator(o, "/=", "signed", "signed", "boolean");
        _parseOperator(o, "/=", "natural", "unsigned", "boolean");
        _parseOperator(o, "/=", "integer", "signed", "boolean");
        _parseOperator(o, "/=", "unsigned", "natural", "boolean");
        _parseOperator(o, "/=", "signed", "integer", "boolean");
        _parseOperator(o, "sll", "unsigned", "integer", "unsigned");
        _parseOperator(o, "sll", "signed", "integer", "signed");
        _parseOperator(o, "srl", "unsigned", "integer", "unsigned");
        _parseOperator(o, "srl", "signed", "integer", "signed");
        _parseOperator(o, "rol", "unsigned", "integer", "unsigned");
        _parseOperator(o, "rol", "signed", "integer", "signed");
        _parseOperator(o, "ror", "unsigned", "integer", "unsigned");
        _parseOperator(o, "ror", "signed", "integer", "signed");
        _parseOperator(o, "not", "unsigned", "unsigned");
        _parseOperator(o, "and", "unsigned", "unsigned", "unsigned");
        _parseOperator(o, "or", "unsigned", "unsigned", "unsigned");
        _parseOperator(o, "nand", "unsigned", "unsigned", "unsigned");
        _parseOperator(o, "nor", "unsigned", "unsigned", "unsigned");
        _parseOperator(o, "xor", "unsigned", "unsigned", "unsigned");
        _parseOperator(o, "xnor", "unsigned", "unsigned", "unsigned");
        _parseOperator(o, "not", "signed", "signed");
        _parseOperator(o, "and", "signed", "signed", "signed");
        _parseOperator(o, "or", "signed", "signed", "signed");
        _parseOperator(o, "nand", "signed", "signed", "signed");
        _parseOperator(o, "nor", "signed", "signed", "signed");
        _parseOperator(o, "xor", "signed", "signed", "signed");
        _parseOperator(o, "xnor", "signed", "signed", "signed");
    } else if (o->getName() == "ieee_std_logic_signed") {
        //_parseOperator(o, "+", "std_logic_vector", "std_logic_vector", "std_logic_vector");
        _parseOperator(o, "+", "std_logic_vector", "integer", "std_logic_vector");
        _parseOperator(o, "+", "integer", "std_logic_vector", "std_logic_vector");
        _parseOperator(o, "+", "std_logic_vector", "std_logic", "std_logic_vector");
        _parseOperator(o, "+", "std_logic", "std_logic_vector", "std_logic_vector");
        //_parseOperator(o, "-", "std_logic_vector", "std_logic_vector", "std_logic_vector");
        _parseOperator(o, "-", "std_logic_vector", "integer", "std_logic_vector");
        _parseOperator(o, "-", "integer", "std_logic_vector", "std_logic_vector");
        _parseOperator(o, "-", "std_logic_vector", "std_logic", "std_logic_vector");
        _parseOperator(o, "-", "std_logic", "std_logic_vector", "std_logic_vector");
        _parseOperator(o, "+", "std_logic_vector", "std_logic_vector");
        _parseOperator(o, "-", "std_logic_vector", "std_logic_vector");
        //_parseOperator(o, "abs", "std_logic_vector", "std_logic_vector");
        //_parseOperator(o, "*", "std_logic_vector", "std_logic_vector", "std_logic_vector");
        _parseOperator(o, "<", "std_logic_vector", "std_logic_vector", "boolean");
        _parseOperator(o, "<", "std_logic_vector", "integer", "boolean");
        _parseOperator(o, "<", "integer", "std_logic_vector", "boolean");
        _parseOperator(o, "<=", "std_logic_vector", "std_logic_vector", "boolean");
        _parseOperator(o, "<=", "std_logic_vector", "integer", "boolean");
        _parseOperator(o, "<=", "integer", "std_logic_vector", "boolean");
        _parseOperator(o, ">", "std_logic_vector", "std_logic_vector", "boolean");
        _parseOperator(o, ">", "std_logic_vector", "integer", "boolean");
        _parseOperator(o, ">", "integer", "std_logic_vector", "boolean");
        _parseOperator(o, ">=", "std_logic_vector", "std_logic_vector", "boolean");
        _parseOperator(o, ">=", "std_logic_vector", "integer", "boolean");
        _parseOperator(o, ">=", "integer", "std_logic_vector", "boolean");
        _parseOperator(o, "=", "std_logic_vector", "std_logic_vector", "boolean");
        _parseOperator(o, "=", "std_logic_vector", "integer", "boolean");
        _parseOperator(o, "=", "integer", "std_logic_vector", "boolean");
        _parseOperator(o, "/=", "std_logic_vector", "std_logic_vector", "boolean");
        _parseOperator(o, "/=", "std_logic_vector", "integer", "boolean");
        _parseOperator(o, "/=", "integer", "std_logic_vector", "boolean");
    } else if (o->getName() == "ieee_std_logic_unsigned") {
        //_parseOperator(o, "+", "std_logic_vector", "std_logic_vector", "std_logic_vector");
        _parseOperator(o, "+", "std_logic_vector", "integer", "std_logic_vector");
        _parseOperator(o, "+", "integer", "std_logic_vector", "std_logic_vector");
        _parseOperator(o, "+", "std_logic_vector", "std_logic", "std_logic_vector");
        _parseOperator(o, "+", "std_logic", "std_logic_vector", "std_logic_vector");
        //_parseOperator(o, "-", "std_logic_vector", "std_logic_vector", "std_logic_vector");
        _parseOperator(o, "-", "std_logic_vector", "integer", "std_logic_vector");
        _parseOperator(o, "-", "integer", "std_logic_vector", "std_logic_vector");
        _parseOperator(o, "-", "std_logic_vector", "std_logic", "std_logic_vector");
        _parseOperator(o, "-", "std_logic", "std_logic_vector", "std_logic_vector");
        _parseOperator(o, "+", "std_logic_vector", "std_logic_vector");
        //_parseOperator(o, "*", "std_logic_vector", "std_logic_vector", "std_logic_vector");
        _parseOperator(o, "<", "std_logic_vector", "std_logic_vector", "boolean");
        _parseOperator(o, "<", "std_logic_vector", "integer", "boolean");
        _parseOperator(o, "<", "integer", "std_logic_vector", "boolean");
        _parseOperator(o, "<=", "std_logic_vector", "std_logic_vector", "boolean");
        _parseOperator(o, "<=", "std_logic_vector", "integer", "boolean");
        _parseOperator(o, "<=", "integer", "std_logic_vector", "boolean");
        _parseOperator(o, ">", "std_logic_vector", "std_logic_vector", "boolean");
        _parseOperator(o, ">", "std_logic_vector", "integer", "boolean");
        _parseOperator(o, ">", "integer", "std_logic_vector", "boolean");
        _parseOperator(o, ">=", "std_logic_vector", "std_logic_vector", "boolean");
        _parseOperator(o, ">=", "std_logic_vector", "integer", "boolean");
        _parseOperator(o, ">=", "integer", "std_logic_vector", "boolean");
        _parseOperator(o, "=", "std_logic_vector", "std_logic_vector", "boolean");
        _parseOperator(o, "=", "std_logic_vector", "integer", "boolean");
        _parseOperator(o, "=", "integer", "std_logic_vector", "boolean");
        _parseOperator(o, "/=", "std_logic_vector", "std_logic_vector", "boolean");
        _parseOperator(o, "/=", "std_logic_vector", "integer", "boolean");
        _parseOperator(o, "/=", "integer", "std_logic_vector", "boolean");
    }
}

void PostParsingVisitor_step2::_removeStandardOperatorOverloads()
{
    for (auto *sub : _operators) {
        sub->replace(nullptr);
        delete sub;
    }
}

void PostParsingVisitor_step2::_parseOperator(
    LibraryDef *ld,
    const std::string &op,
    const std::string &param1,
    const std::string &param2,
    const std::string &param3)
{
    auto *sub = new Function();

    // Operator name:
    if (op == "abs") {
        sub->setName("__vhdl_abs");
    } else if (op == "+") {
        sub->setName("__vhdl_op_plus");
    } else if (op == "*") {
        sub->setName("__vhdl_op_mult");
    } else if (op == "-") {
        sub->setName("__vhdl_op_minus");
    } else if (op == "/") {
        sub->setName("__vhdl_op_div");
    } else if (op == "**") {
        sub->setName("__vhdl_op_pow");
    } else if (op == "=") {
        sub->setName("__vhdl_op_eq");
    } else if (op == "/=") {
        sub->setName("__vhdl_op_neq");
    } else if (op == ">") {
        sub->setName("__vhdl_op_gt");
    } else if (op == "<") {
        sub->setName("__vhdl_op_lt");
    } else if (op == ">=") {
        sub->setName("__vhdl_op_ge");
    } else if (op == "<=") {
        sub->setName("__vhdl_op_le");
    } else if (op == "mod") {
        sub->setName("__vhdl_op_mod");
    } else if (op == "rem") {
        sub->setName("__vhdl_op_rem");
    } else if (op == "sll") {
        sub->setName("__vhdl_op_sll");
    } else if (op == "srl") {
        sub->setName("__vhdl_op_srl");
    } else if (op == "sla") {
        sub->setName("__vhdl_op_sla");
    } else if (op == "sra") {
        sub->setName("__vhdl_op_sra");
    } else if (op == "rol") {
        sub->setName("__vhdl_op_rol");
    } else if (op == "ror") {
        sub->setName("__vhdl_op_ror");
    } else if (op == "not") {
        sub->setName("__vhdl_op_bnot");
    } else if (op == "and") {
        sub->setName("__vhdl_op_band");
    } else if (op == "or") {
        sub->setName("__vhdl_op_bor");
    } else if (op == "xor") {
        sub->setName("__vhdl_op_bxor");
    } else if (op == "&") {
        sub->setName("__vhdl_op_concat");
        // Listed, but not supported:
    } else if (op == "nor") {
        sub->setName("__vhdl_op_nor");
    } else if (op == "nand") {
        sub->setName("__vhdl_op_nand");
    } else if (op == "xnor") {
        sub->setName("__vhdl_op_xnor");
    } else {
        messageError("Unexpected operator: " + op, nullptr, nullptr);
    }

    _makeParam(sub, "1", param1);
    if (param3.empty()) {
        _makeReturnType(sub, param2);
    } else {
        _makeParam(sub, "2", param2);
        _makeReturnType(sub, param3);
    }

    ld->declarations.push_back(sub);
    _operators.insert(sub);
}

void PostParsingVisitor_step2::_makeParam(Function *sub, const std::string &pos, const std::string &type)
{
    auto *p = new Parameter();
    p->setName("param" + pos);
    Type *t = _makeType(sub, type, pos);
    p->setType(t);
    sub->parameters.push_back(p);
}

void PostParsingVisitor_step2::_makeReturnType(Function *sub, const std::string &type)
{
    Type *t = _makeType(sub, type, "0");
    sub->setType(t);
}

auto PostParsingVisitor_step2::_makeType(Function *sub, const std::string &type, const std::string &pos) -> Type *
{
    Type *ret        = nullptr;
    bool addTemplate = false;
    if (type == "signed") {
        Range *retSpan = nullptr;
        if (pos == "0") {
            // NOTE: span will be not used for returned type
            retSpan = _factory.range(31, 0);

        } else {
            retSpan     = _factory.range(new Identifier("left" + pos), dir_downto, new Identifier("right" + pos));
            addTemplate = true;
        }
        ret = _factory.signedType(retSpan);
    } else if (type == "unsigned") {
        Range *retSpan = nullptr;
        if (pos == "0") {
            // NOTE: span will be not used for returned type
            retSpan = _factory.range(31, 0);

        } else {
            retSpan     = _factory.range(new Identifier("left" + pos), dir_downto, new Identifier("right" + pos));
            addTemplate = true;
        }
        ret = _factory.unsignedType(retSpan);
    } else if (type == "integer") {
        ret = _factory.integer();
    } else if (type == "natural") {
        ret = _factory.integer();
    } else if (type == "std_logic_vector") {
        Range *retSpan = nullptr;
        if (pos == "0") {
            // NOTE: span will be not used for returned type
            retSpan = _factory.range(31, 0);

        } else {
            retSpan     = _factory.range(new Identifier("left" + pos), dir_downto, new Identifier("right" + pos));
            addTemplate = true;
        }
        ret = _factory.bitvector(retSpan, true, true);
    } else if (type == "std_ulogic_vector") {
        Range *retSpan = nullptr;
        if (pos == "0") {
            // NOTE: span will be not used for returned type
            retSpan = _factory.range(31, 0);

        } else {
            retSpan     = _factory.range(new Identifier("left" + pos), dir_downto, new Identifier("right" + pos));
            addTemplate = true;
        }
        ret = _factory.bitvector(retSpan, true, false);
    } else if (type == "std_logic") {
        ret = _factory.bit(true, true);
    } else if (type == "std_ulogic") {
        ret = _factory.bit(true, false);
    } else if (type == "boolean") {
        ret = _factory.boolean();
    } else if (type == "ux01") {
        ret = _factory.typeRef("ux01");
    } else {
        messageError("Unexpected type: " + type, nullptr, nullptr);
    }

    if (addTemplate) {
        sub->templateParameters.push_back(_factory.templateValueParameter(_factory.integer(), ("left" + pos)));
        sub->templateParameters.push_back(_factory.templateValueParameter(_factory.integer(), ("right" + pos)));
    }

    return ret;
}

auto PostParsingVisitor_step2::_getCastedType(Cast &o) -> Type *
{
    Value *castOperand = o.getValue();
    if (castOperand == nullptr) {
        messageError("Cast operand not present in: ", &o, _sem);
    }

    Type *castType = hif::semantics::getSemanticType(castOperand, _sem);
    if (castType == nullptr) {
        messageError("Type not found for cast operand: ", castOperand, _sem);
    }

    // If it is a TypeReference get the base type.
    if (dynamic_cast<TypeReference *>(castType) != nullptr) {
        Type *tmp = hif::semantics::getBaseType(castType, false, _sem);
        if (tmp == nullptr) {
            messageError("Base type not found for cast operand: ", castType, _sem);
        }
        castType = tmp;
    }

    return castType;
}

void PostParsingVisitor_step2::_manageCast(Cast &o)
{
    Type *bt  = hif::semantics::getBaseType(o.getType(), false, _sem);
    Int *i    = dynamic_cast<Int *>(bt);
    auto *s   = dynamic_cast<Signed *>(o.getType());
    auto *us  = dynamic_cast<Unsigned *>(o.getType());
    auto *arr = dynamic_cast<Array *>(o.getType());
    auto *bv  = dynamic_cast<Bitvector *>(o.getType());

    if (i == nullptr && s == nullptr && us == nullptr && arr == nullptr && bv == nullptr) {
        return;
    }

    if (i != nullptr) {
        Real *r = dynamic_cast<Real *>(hif::semantics::getSemanticType(o.getValue(), _sem));
        if (r == nullptr) {
            return;
        }

        // mapping cast into call to hif_mapRealToInt
        Value *is      = hif::semantics::spanGetSize(i->getSpan(), _sem);
        BoolValue *bvv = _factory.boolval(i->isSigned());

        Value *v           = o.setValue(nullptr);
        FunctionCall *call = _factory.functionCall(
            "castRealToInt", _factory.libraryInstance("standard", false, true), _factory.noTemplateArguments(),
            (_factory.parameterArgument("param1", v), _factory.parameterArgument("param2", is),
             _factory.parameterArgument("param3", bvv)));
        o.setValue(call);

        return;
    }

    if (hif::typeGetSpan(o.getType(), _sem) != nullptr) {
        return;
    }

    Type *castType = _getCastedType(o);
    messageAssert(castType != nullptr, "Cannot get casted type", &o, _sem);

    hif::typeSetSpan(o.getType(), hif::copy(hif::typeGetSpan(castType, _sem)), _sem);
    hif::typeSetSigned(o.getType(), hif::typeIsSigned(castType, _sem), _sem);
}

void PostParsingVisitor_step2::_fixPortPartialBindings(Partials &partials)
{
    messageDebugAssert(!partials.empty(), "Partial bind not found.", nullptr, nullptr);

    hif::analysis::IndexMap indexMap;
    PortAssign *first = *partials.begin();
    Type *paType      = hif::semantics::getSemanticType(first, _sem);
    messageAssert(paType != nullptr, "Cannot type description", first, _sem);

    const bool hasTemplates = hif::typeDependsOnTemplates(paType, _sem);
    Value *paTypeMin        = nullptr;
    if (hasTemplates) {
        Range *paTypeRange = hif::typeGetSpan(paType, _sem);
        paTypeMin          = hif::rangeGetMinBound(paTypeRange);
    }

    // Collecting info
    hif::Trash trash;
    for (auto *pa : partials) {
        hif::analysis::IndexInfo index;

        Value *partial = pa->getPartialBind();
        auto *r        = dynamic_cast<Range *>(partial);
        if (r == nullptr) {
            index.expression = _getPartial(partial, paTypeMin, trash, hasTemplates);
        } else {
            index.slice = _getPartial(r, paTypeMin, trash, hasTemplates);
        }

        indexMap[index] = pa->getValue();
    }

    // Setting of others.
    Type *elementType = hif::semantics::getVectorElementType(paType, _sem);
    messageAssert(elementType != nullptr, "Unexpected partial bind port type", paType, _sem);

    Port *port = hif::semantics::getDeclaration(first, _sem);
    messageAssert(port != nullptr, "Declaration not found", first, _sem);
    Value *others = _sem->getTypeDefaultValue(elementType, port);

    Value *concat = hif::manipulation::createConcatFromSpans(paType, indexMap, _sem, others);
    delete elementType;
    delete others;
    trash.clear();

    messageAssert(concat != nullptr, "Cannot resolve partial binding", first, _sem);

    for (auto *pa : partials) {
        if (pa == first) {
            continue;
        }
        pa->replace(nullptr);
        delete pa;
    }

    delete first->setPartialBind(nullptr);
    delete first->setValue(concat);
}

auto PostParsingVisitor_step2::_getPartial(Value *index, Value *min, Trash &trash, const bool hasTemplates) -> Value *
{
    if (!hasTemplates) {
        return index;
    }

    Value *simplifiedMin = hif::copy(min);

    hif::manipulation::SimplifyOptions opt;
    opt.simplify_template_parameters = true;
    opt.simplify_constants           = true;
    simplifiedMin                    = hif::manipulation::simplify(simplifiedMin, _sem, opt);

    Expression *e =
        _factory.expression(_factory.expression(hif::copy(index), op_minus, simplifiedMin), op_plus, hif::copy(min));
    Value *ret = hif::manipulation::simplify(e, _sem);
    trash.insert(ret);
    return ret;
}

auto PostParsingVisitor_step2::_getPartial(Range *index, Value *min, Trash &trash, const bool hasTemplates) -> Range *
{
    if (!hasTemplates) {
        return index;
    }
    auto *ret = new Range();
    ret->setDirection(index->getDirection());
    ret->setLeftBound(_getPartial(index->getLeftBound(), min, trash, hasTemplates));
    ret->setRightBound(_getPartial(index->getRightBound(), min, trash, hasTemplates));

    trash.insert(ret);
    return ret;
}

auto PostParsingVisitor_step2::_fixPartialBindings(Instance *inst) -> bool
{
    PartialNames partialNames;
    for (BList<PortAssign>::iterator it(inst->portAssigns.begin()); it != inst->portAssigns.end(); ++it) {
        PortAssign *pass = *it;
        if (pass->getPartialBind() == nullptr) {
            continue;
        }
        partialNames[pass->getName()].insert(pass);
    }

    if (partialNames.empty()) {
        return false;
    }

    // Each name represents a set of partial bindings to fix.
    for (auto &partialName : partialNames) {
        _fixPortPartialBindings(partialName.second);
    }

    return true;
}

} // namespace

void performStep2Refinements(hif::System *o, hif::semantics::ILanguageSemantics *sem)
{
    hif::application_utils::initializeLogHeader("VHDL2HIF", "performStep2Refinements");

    PostParsingVisitor_step2 v(sem);
    o->acceptVisitor(v);

    // Tree is now stable.

    // Fixing eventual conflicting names:
    hif::manipulation::renameConflictingDeclarations(o, sem, nullptr, "inst");

    hif::application_utils::restoreLogHeader();
}

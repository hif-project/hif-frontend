/// @file VhdlParser_psl.cpp
/// @brief
/// Copyright (c) 2024-2025, Electronic Systems Design (ESD) Group,
/// Univeristy of Verona.
/// This file is distributed under the BSD 2-Clause License.
/// See LICENSE.md for details.

#include "vhdl2hif/vhdl_parser.hpp"
#include "vhdl2hif/vhdl_support.hpp"

using namespace hif;

/*
 * PSL-Grammar related functions
 * --------------------------------------------------------------------- */

auto VhdlParser::parse_RangePsl(Value *lower, Value *upper) -> Range *
{
    auto *ret = new Range(lower, upper, dir_upto);
    setCodeInfo(ret);

    return ret;
}

auto VhdlParser::parse_AssertDirective(Value *property, Value *report, Value *severity) -> assert_directive_t *
{
    auto *ret = new assert_directive_t();

    ret->property = property;
    ret->report   = report;
    ret->severity = severity;

    return ret;
}

auto VhdlParser::parse_FLProperty(Value *hdl_or_psl_expression) -> Value * { return hdl_or_psl_expression; }

auto VhdlParser::parse_FLProperty(Value *fl_property, Value *clock_expression) -> Value *
{
    auto *ret = new FunctionCall();
    setCodeInfo(ret);
    ret->setName("psl_fl_property");
    Instance *inst = _factory.libraryInstance("psl_standard", false, true);
    ret->setInstance(inst);

    auto *p1 = new ParameterAssign();
    setCodeInfo(p1);
    p1->setName("op");
    p1->setValue(new Identifier("psl_at_clause"));
    ret->parameterAssigns.push_back(p1);

    auto *p2 = new ParameterAssign();
    setCodeInfo(p2);
    p2->setName("fl_property1");
    p2->setValue(fl_property);
    ret->parameterAssigns.push_back(p2);

    auto *p3 = new ParameterAssign();
    setCodeInfo(p3);
    p3->setName("fl_property2");
    p3->setValue(clock_expression);
    ret->parameterAssigns.push_back(p3);

    return ret;
}

auto VhdlParser::parse_FLProperty(const char *op, Value *fl_property) -> Value *
{
    auto *ret = new FunctionCall();
    setCodeInfo(ret);
    ret->setName("psl_fl_property");
    Instance *inst = _factory.libraryInstance("psl_standard", false, true);
    ret->setInstance(inst);

    auto *p1 = new ParameterAssign();
    setCodeInfo(p1);
    p1->setName("op");
    p1->setValue(new Identifier(op));
    ret->parameterAssigns.push_back(p1);

    auto *p2 = new ParameterAssign();
    setCodeInfo(p2);
    p2->setName("fl_property");
    p2->setValue(fl_property);
    ret->parameterAssigns.push_back(p2);

    return ret;
}

auto VhdlParser::parse_FLPropertyCycles(const char *op, Value *fl_property, Value *cycles) -> Value *
{
    auto *ret = dynamic_cast<FunctionCall *>(parse_FLProperty(op, fl_property));

    auto *p1 = new ParameterAssign();
    setCodeInfo(p1);
    p1->setName("cycles");
    p1->setValue(cycles);
    ret->parameterAssigns.push_back(p1);

    return ret;
}

auto VhdlParser::parse_FLPropertyRange(const char *op, Value *fl_property, Range *range) -> Value *
{
    auto *ret = dynamic_cast<FunctionCall *>(parse_FLProperty(op, fl_property));

    auto *p1 = new ParameterAssign();
    setCodeInfo(p1);
    p1->setName("range_lbound");
    p1->setValue(hif::copy(range->getLeftBound()));
    ret->parameterAssigns.push_back(p1);

    auto *p2 = new ParameterAssign();
    setCodeInfo(p2);
    p2->setName("range_rbound");
    p2->setValue(hif::copy(range->getRightBound()));
    ret->parameterAssigns.push_back(p2);

    delete range;

    return ret;
}

auto VhdlParser::parse_FLPropertyOccurrence(const char *op, Value *fl_property, Value *occurrence) -> Value *
{
    auto *ret = dynamic_cast<FunctionCall *>(parse_FLProperty(op, fl_property));

    auto *p1 = new ParameterAssign();
    setCodeInfo(p1);
    p1->setName("occurrence_expression");
    p1->setValue(occurrence);
    ret->parameterAssigns.push_back(p1);

    return ret;
}

auto VhdlParser::parse_FLPropertyOccurrenceCycles(const char *op, Value *fl_property, Value *occurrence, Value *cycles)
    -> Value *
{
    auto *ret = dynamic_cast<FunctionCall *>(parse_FLProperty(op, fl_property));

    auto *p1 = new ParameterAssign();
    setCodeInfo(p1);
    p1->setName("occurrence_expression");
    p1->setValue(occurrence);
    ret->parameterAssigns.push_back(p1);

    auto *p2 = new ParameterAssign();
    setCodeInfo(p2);
    p2->setName("cycles");
    p2->setValue(cycles);
    ret->parameterAssigns.push_back(p2);

    return ret;
}

auto VhdlParser::parse_FLPropertyOccurrenceRange(const char *op, Value *fl_property, Value *occurrence, Range *range)
    -> Value *
{
    auto *ret = dynamic_cast<FunctionCall *>(parse_FLProperty(op, fl_property));

    auto *p1 = new ParameterAssign();
    setCodeInfo(p1);
    p1->setName("occurrence_expression");
    p1->setValue(occurrence);
    ret->parameterAssigns.push_back(p1);

    auto *p2 = new ParameterAssign();
    setCodeInfo(p2);
    p2->setName("range_lbound");
    p2->setValue(hif::copy(range->getLeftBound()));
    ret->parameterAssigns.push_back(p2);

    auto *p3 = new ParameterAssign();
    setCodeInfo(p3);
    p3->setName("range_rbound");
    p3->setValue(hif::copy(range->getRightBound()));
    ret->parameterAssigns.push_back(p3);

    delete range;

    return ret;
}

auto VhdlParser::parse_FLProperty(const char *op, Value *fl_property1, Value *fl_property2) -> Value *
{
    auto *ret = new FunctionCall();
    setCodeInfo(ret);
    ret->setName("psl_fl_property");
    Instance *inst = _factory.libraryInstance("psl_standard", false, true);
    ret->setInstance(inst);

    auto *p1 = new ParameterAssign();
    setCodeInfo(p1);
    p1->setName("op");
    p1->setValue(new Identifier(op));
    ret->parameterAssigns.push_back(p1);

    auto *p2 = new ParameterAssign();
    setCodeInfo(p2);
    p2->setName("fl_property1");
    p2->setValue(fl_property1);
    ret->parameterAssigns.push_back(p2);

    auto *p3 = new ParameterAssign();
    setCodeInfo(p3);
    p3->setName("fl_property2");
    p3->setValue(fl_property2);
    ret->parameterAssigns.push_back(p3);

    return ret;
}

auto VhdlParser::parse_PslDirective(verification_directive_t *v) -> ProcedureCall *
{
    ProcedureCall *ret = nullptr;

    if (v->assert_directive != nullptr) {
        assert_directive_t *assert_directive = v->assert_directive;

        ret = new ProcedureCall();
        ret->setName("psl_assert");
        Instance *inst = _factory.libraryInstance("psl_standard", false, true);
        ret->setInstance(inst);

        auto *p1 = new ParameterAssign();
        setCodeInfo(p1);
        p1->setName("property");
        p1->setValue(assert_directive->property);
        ret->parameterAssigns.push_back(p1);

        if (assert_directive->report != nullptr) {
            auto *p2 = new ParameterAssign();
            setCodeInfo(p2);
            p2->setName("report");
            p2->setValue(assert_directive->report);
            ret->parameterAssigns.push_back(p2);
        }

        ret->setSourceFileName(assert_directive->property->getSourceFileName());
        ret->setSourceLineNumber(assert_directive->property->getSourceLineNumber());
        ret->setSourceColumnNumber(assert_directive->property->getSourceColumnNumber());

        delete assert_directive;
    }
    //    else if (v->assume_directive != nullptr)
    //    {

    //    }
    //    else if (v->restrict_directive != nullptr)
    //    {

    //    }
    //    else if (v->restrict_excl_directive != nullptr)
    //    {

    //    }
    //    else if (v->cover_directive != nullptr)
    //    {

    //    }
    //    else if (v->fairness_statement != nullptr)
    //    {

    //    }
    else {
        messageAssert(false, "Unexpected verification directive", nullptr, nullptr);
    }

    delete v;

    return ret;
}

auto VhdlParser::parse_VerificationUnit(Value *name, std::list<vunit_item_t *> *items) -> DesignUnit *
{
    auto *vunitName = dynamic_cast<Identifier *>(name);
    messageAssert(vunitName != nullptr, "Unexpected vunit identifier", name, nullptr);

    auto *designUnit_o = new DesignUnit();
    auto *contents_o   = new Contents();
    View *view_o       = new View();
    auto *stateTable_o = new StateTable();
    auto *state_o      = new State();

    view_o->setEntity(new Entity());
    view_o->setLanguageID(hif::psl);

    stateTable_o->setName(vunitName->getName());
    designUnit_o->setName(vunitName->getName());
    view_o->setName(vunitName->getName());

    setCodeInfoFromCurrentBlock(designUnit_o);
    setCodeInfoFromCurrentBlock(contents_o);
    setCodeInfoFromCurrentBlock(view_o);
    setCodeInfoFromCurrentBlock(state_o);
    setCodeInfoFromCurrentBlock(stateTable_o);

    std::list<concurrent_statement_t *> concurrent_statement_list;

    // STEP 1 : add psl directives and declarations
    for (auto it = items->begin(); it != items->end();) {
        vunit_item_t *item = *it;

        if (item->concurrent_statement != nullptr) {
            concurrent_statement_list.push_back((*it)->concurrent_statement);
        } else if (item->psl_directive != nullptr) {
            state_o->actions.push_back((*it)->psl_directive);
        } else if (item->hdl_decl != nullptr) {
            block_declarative_item_t *decl = item->hdl_decl;

            if (decl->declarations != nullptr) {
                contents_o->declarations.merge(*decl->declarations);
                delete decl->declarations;
            } else if (decl->use_clause != nullptr) {
                contents_o->libraries.merge(*decl->use_clause);
                delete decl->use_clause;
            } else if (decl->component_declaration != nullptr) {
                messageError(
                    "Component declaration inside verification units "
                    "are not supported",
                    nullptr, nullptr);
            } else if (decl->configuration_specification != nullptr) {
                messageError(
                    "Configuration specifications inside verification "
                    "units are not supported",
                    nullptr, nullptr);
            } else {
                messageError("Unexpected declaration inside verification unit", nullptr, nullptr);
            }
        } else {
            messageAssert(false, "Unexpected vunit item", nullptr, nullptr);
        }

        delete *it;
        it = items->erase(it);
    }

    // STEP 2 : add concurrent statements
    _populateContents(contents_o, &concurrent_statement_list);

    // STEP 3 : add use-clause
    Identifier libId("psl_standard");
    view_o->libraries.push_back(resolveLibraryType(&libId));

    // Finally, build the DesignUnit
    stateTable_o->states.push_back(state_o);
    contents_o->stateTables.push_back(stateTable_o);
    view_o->setContents(contents_o);
    designUnit_o->views.push_back(view_o);
    du_declarations->push_back(designUnit_o);

    delete name;

    return designUnit_o;
}

void VhdlParser::parse_VerificationUnit(Value *name, ViewReference *context_spec, std::list<vunit_item_t *> *items)
{
    DesignUnit *du_o = parse_VerificationUnit(name, items);

    BList<View>::iterator it = du_o->views.begin();
    while (it != du_o->views.end()) {
        View *view_o = *it;
        view_o->inheritances.push_back(context_spec);
        ++it;
    }
}

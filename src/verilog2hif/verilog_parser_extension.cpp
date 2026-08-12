/// @file verilog_parser_extension.cpp
/// @brief
/// Copyright (c) 2024-2025, Electronic Systems Design (ESD) Group,
/// Univeristy of Verona.
/// This file is distributed under the BSD 2-Clause License.
/// See LICENSE.md for details.

#include <cmath>

#include <algorithm>
#include <sstream>

#include "verilog2hif/support.hpp"
#include "verilog2hif/verilog_parser.hpp"

using namespace hif;
using std::list;
using std::stringstream;

bool VerilogParser::_isVerilogAms = false;

auto getTimeValue(std::string s) -> TimeValue *
{
    typedef std::string String;
    typedef String::size_type Size;

    auto *ret = new TimeValue();
    Size i    = 0;
    for (; i < s.size(); ++i) {
        char c = s[i];
        if (c != '0' && c != '1') {
            break;
        }
    }

    String val  = s.substr(0, i);
    String unit = s.substr(i);

    double dval = NAN;

    stringstream ss;
    ss << val;
    ss >> dval;

    TimeValue::TimeUnit uval = TimeValue::time_fs;

    if (unit == "s") {
        uval = TimeValue::time_sec;
    } else if (unit == "ms") {
        uval = TimeValue::time_ms;
    } else if (unit == "us") {
        uval = TimeValue::time_us;
    } else if (unit == "ns") {
        uval = TimeValue::time_ns;
    } else if (unit == "ps") {
        uval = TimeValue::time_ps;
    } else if (unit == "fs") {
        uval = TimeValue::time_fs;
    } else {
        messageError("Unexpected unit for timescale " + unit, nullptr, nullptr);
    }

    ret->setValue(dval);
    ret->setUnit(uval);

    return ret;
}

void parse_timescale(std::string ts);

void parse_timescale(std::string ts)
{
    typedef std::string String;
    typedef String::size_type Size;
    extern VerilogParser *parserInstance;

    ts = ts.substr(10); // removing `timescale
    // removing spaces  \t\b\f\r
    ts.erase(std::remove(ts.begin(), ts.end(), ' '), ts.end());
    ts.erase(std::remove(ts.begin(), ts.end(), '\t'), ts.end());
    ts.erase(std::remove(ts.begin(), ts.end(), '\b'), ts.end());
    ts.erase(std::remove(ts.begin(), ts.end(), '\f'), ts.end());
    ts.erase(std::remove(ts.begin(), ts.end(), '\r'), ts.end());

    Size it          = ts.find('/');
    String unit      = ts.substr(0, it);
    String precision = ts.substr(it + 1);

    TimeValue *tvUnit      = getTimeValue(unit);
    TimeValue *tvPrecision = getTimeValue(precision);

    parserInstance->SetTimescale(tvUnit, tvPrecision);
}

void VerilogParser::SetTimescale(TimeValue *unit, TimeValue *precision)
{
    delete _unit;
    delete _precision;
    _unit      = unit;
    _precision = precision;
}

auto VerilogParser::getFactory() -> HifFactory * { return &_factory; }

void VerilogParser::_buildActionList(
    statement_t *statement,
    BList<Action> &actionList,
    hif::BList<Declaration> *declList)
{
    _buildActionListFromStatement(statement, actionList, declList);
}

void VerilogParser::_buildActionList(analog_statement_t *statement, hif::BList<Action> &actionList)
{
    _buildActionListFromAnalogStatement(statement, actionList);
}

void VerilogParser::_buildActionListFromStatement(
    statement_t *statement,
    BList<Action> &actionList,
    hif::BList<Declaration> *declList)
{
    if (statement == nullptr) {
        return;
    }

    if (statement->seq_block_declarations != nullptr) {
        if (!statement->seq_block_declarations->empty()) {
            if (declList == nullptr) {
                messageError(
                    "Declarations inside sequential blocks are not supported",
                    statement->seq_block_declarations->front(), nullptr);
            } else {
                declList->merge(*statement->seq_block_declarations);
            }
        }

        delete statement->seq_block_declarations;
        statement->seq_block_declarations = nullptr;
    }

    if (statement->procedural_timing_control != nullptr) {
        procedural_timing_control_t *ptc = statement->procedural_timing_control;

        if (ptc->delay_control != nullptr) {
            Value *dc    = ptc->delay_control;
            Wait *wait_o = new Wait();
            setCodeInfo(wait_o);
            wait_o->setTime(dc);
            actionList.push_back(wait_o);
        } else if (ptc->event_control != nullptr) {
            event_control_t *ect = ptc->event_control;

            Wait *wait_o = new Wait();
            setCodeInfo(wait_o);

            bool allSignals = false;
            _buildSensitivityFromEventControl(ect, wait_o->sensitivity, allSignals);

            // Fixing sensitivity:
            for (BList<Value>::iterator it = wait_o->sensitivity.begin(); it != wait_o->sensitivity.end();) {
                Value *v = *it;
                if (v->checkProperty(PROPERTY_SENSITIVE_POS)) {
                    it = it.remove();
                    v->removeProperty(PROPERTY_SENSITIVE_POS);
                    wait_o->sensitivityPos.push_back(v);
                } else if (v->checkProperty(PROPERTY_SENSITIVE_NEG)) {
                    it = it.remove();
                    v->removeProperty(PROPERTY_SENSITIVE_NEG);
                    wait_o->sensitivityNeg.push_back(v);
                } else {
                    ++it;
                }
            }

            if (allSignals) {
                // TODO
                messageAssert(false, "Unsupported case", nullptr, nullptr);
            }

            actionList.push_back(wait_o);

            delete ect;
            ptc->event_control = nullptr;
        }

        delete ptc;
        statement->procedural_timing_control = nullptr;
    }

    // Mutually exclusive cases
    if (statement->skipped) {
        return;
    }

    if (statement->blocking_assignment != nullptr) {
        actionList.push_back(statement->blocking_assignment);
    } else if (statement->case_statement != nullptr) {
        actionList.push_back(statement->case_statement);
    } else if (statement->conditional_statement != nullptr) {
        actionList.push_back(statement->conditional_statement);
    } else if (statement->nonblocking_assignment != nullptr) {
        actionList.push_back(statement->nonblocking_assignment);
    } else if (statement->loop_statement != nullptr) {
        actionList.push_back(statement->loop_statement);
    } else if (statement->system_task_enable != nullptr) {
        actionList.push_back(statement->system_task_enable);
    } else if (statement->task_enable != nullptr) {
        actionList.push_back(statement->task_enable);
    } else if (statement->wait_statement != nullptr) {
        actionList.push_back(statement->wait_statement);
    } else if (statement->seq_block_actions != nullptr) {
        if (statement->seq_block_actions->empty()) {
            delete statement->seq_block_actions;
            statement->seq_block_actions = nullptr;
            return;
        }

        std::string blockName = statement->blockName;
        BList<Action> blockActionList;

        auto it = statement->seq_block_actions->begin();
        while (it != statement->seq_block_actions->end()) {
            _buildActionListFromStatement(*it, blockActionList);

            delete *it;
            it = statement->seq_block_actions->erase(it);
        }

        delete statement->seq_block_actions;
        statement->seq_block_actions = nullptr;

        While *while_o = _factory.whileLoop(_factory.bitval(bit_zero), _factory.noActions(), blockName, true);
        while_o->actions.merge(blockActionList);
        actionList.push_back(while_o);
    } else if (statement->disable_statement != nullptr) {
        actionList.push_back(statement->disable_statement);
    } else if (statement->event_trigger != nullptr) {
        actionList.push_back(statement->event_trigger);
    }
    //    else if ( statement->par_block != nullptr )
    //    {
    //        actionList.push_back( statement->par_block );
    //    }
    //    else if ( statement->procedural_continuous_assignments != nullptr )
    //    {
    //        actionList.push_back( statement->procedural_continuous_assignments );
    //    }
    //    else if ( statement->task_enable != nullptr )
    //    {
    //        actionList.push_back( statement->task_enable );
    //    }
    else if (statement->procedural_timing_control != nullptr) {
        // do nothing
    }
}

void VerilogParser::_buildActionListFromAnalogStatement(analog_statement_t *statement, hif::BList<Action> &actionList)
{
    if (statement == nullptr) {
        return;
    }

    if (statement->analog_seq_block_declarations != nullptr && !statement->analog_seq_block_declarations->empty()) {
        messageError(
            "Declarations inside sequential blocks are not supported",
            statement->analog_seq_block_declarations->front(), _sem);
    }

    analog_event_control_t *ect = statement->event_control;

    Wait *wait_o = nullptr;
    if (ect != nullptr) {
        wait_o = new Wait();
        setCodeInfo(wait_o);

        bool allSignals = false;
        _buildSensitivityFromAnalogEventControl(ect, wait_o->sensitivity, allSignals);

        // Fixing sensitivity:
        for (BList<Value>::iterator it = wait_o->sensitivity.begin(); it != wait_o->sensitivity.end();) {
            Value *v = *it;
            if (v->checkProperty(PROPERTY_SENSITIVE_POS)) {
                it = it.remove();
                v->removeProperty(PROPERTY_SENSITIVE_POS);
                wait_o->sensitivityPos.push_back(v);
            } else if (v->checkProperty(PROPERTY_SENSITIVE_NEG)) {
                it = it.remove();
                v->removeProperty(PROPERTY_SENSITIVE_NEG);
                wait_o->sensitivityNeg.push_back(v);
            } else {
                ++it;
            }
        }

        if (allSignals) {
            // TODO
            messageAssert(false, "Unsupported case", nullptr, nullptr);
        }

        actionList.push_back(wait_o);
    }

    // null statement, skip it
    if (statement->skipped) {
        return;
    }

    if (statement->analog_case_statement != nullptr) {
        actionList.push_back(statement->analog_case_statement);
    } else if (statement->analog_conditional_statement != nullptr) {
        actionList.push_back(statement->analog_conditional_statement);
    } else if (statement->analog_loop_statement != nullptr) {
        actionList.push_back(statement->analog_loop_statement);
    } else if (statement->system_task_enable != nullptr) {
        actionList.push_back(statement->system_task_enable);
    } else if (statement->analog_seq_block_actions != nullptr) {
        auto it = statement->analog_seq_block_actions->begin();
        BList<Action> blockActionList;
        while (it != statement->analog_seq_block_actions->end()) {
            _buildActionListFromAnalogStatement(*it, blockActionList);

            delete *it;
            it = statement->analog_seq_block_actions->erase(it);
        }

        delete statement->analog_seq_block_actions;
        While *while_o = _factory.whileLoop(_factory.boolval(false), _factory.noActions(), statement->blockName, true);
        while_o->actions.merge(blockActionList);
        actionList.push_back(while_o);
    } else if (statement->analog_procedural_assignment != nullptr) {
        actionList.push_back(statement->analog_procedural_assignment);
    } else if (statement->contribution_statement != nullptr) {
        actionList.push_back(statement->contribution_statement);
    } else if (statement->indirect_contribution_statement != nullptr) {
        actionList.push_back(statement->indirect_contribution_statement);
    }
    //    else if ( statement->analog_event_control_statement != nullptr )
    //    {

    //    }
    //    else if ( statement->disable_statement != nullptr )
    //    {

    //    }
    else {
        messageAssert(false, "Unexpected analog statement", nullptr, nullptr);
    }

    if (wait_o != nullptr) {
        BList<Action>::iterator it(wait_o);
        it.remove();
        wait_o->actions.merge(actionList);
        actionList.push_back(wait_o);
    }
}

void VerilogParser::_buildSensitivityFromEventControl(
    event_control_t *ect,
    hif::BList<Value> &sensitivity,
    bool &allSignals)
{
    if (ect == nullptr) {
        return;
    }

    if (ect->event_expression_list != nullptr) {
        std::list<event_expression_t *> *event_expr = ect->event_expression_list;
        auto it                                     = event_expr->begin();

        for (; it != event_expr->end(); ++it) {
            sensitivity.push_back((*it)->expression);
            sensitivity.push_back((*it)->negedgeExpression);
            sensitivity.push_back((*it)->posedgeExpression);

            delete *it;
        }

        delete event_expr;
    } else if (ect->event_identifier != nullptr) {
        sensitivity.push_back(ect->event_identifier);
    } else if (ect->event_all) {
        allSignals = true;
    }
}

void VerilogParser::_buildSensitivityFromAnalogEventControl(
    analog_event_control_t *ect,
    hif::BList<Value> &sensitivity,
    bool &allSignals)
{
    if (ect == nullptr) {
        return;
    }

    if (ect->analog_event_expression != nullptr) {
        if (ect->analog_event_expression->or_analog_event_expression != nullptr) {
            sensitivity.merge(*ect->analog_event_expression->or_analog_event_expression);
        } else if (ect->analog_event_expression->negedgeExpression != nullptr) {
            sensitivity.push_back(ect->analog_event_expression->negedgeExpression);
        } else if (ect->analog_event_expression->posedgeExpression != nullptr) {
            sensitivity.push_back(ect->analog_event_expression->posedgeExpression);
        } else {
            messageError("Unexpected case", ect->getFirstObject(), _sem);
        }
    } else if (ect->event_expression_list != nullptr) {
        std::list<event_expression_t *> *event_expr = ect->event_expression_list;
        auto it                                     = event_expr->begin();

        for (; it != event_expr->end();) {
            sensitivity.push_back((*it)->expression);
            sensitivity.push_back((*it)->negedgeExpression);
            sensitivity.push_back((*it)->posedgeExpression);

            //delete *it;
            it = event_expr->erase(it);
        }

        delete event_expr;
    } else if (ect->event_identifier != nullptr) {
        sensitivity.push_back(ect->event_identifier);
    } else if (ect->event_all) {
        allSignals = true;
    }
}

void VerilogParser::_manageModuleItemDeclarations(
    BaseContents *contents_o,
    hif::DesignUnit *du,
    module_or_generate_item_declaration_t *decls)
{
    messageAssert((du != nullptr) || (contents_o != nullptr), "Wrong parameters", nullptr, _sem);
    Entity *iface_o = nullptr;
    if (du != nullptr) {
        View *view_o = du->views.back();
        iface_o      = view_o->getEntity();
        contents_o   = view_o->getContents();
    }

    BList<Declaration> *decl_list = nullptr;

    if (decls->event_declaration != nullptr) {
        decl_list = decls->event_declaration;
    } else if (decls->function_declaration != nullptr) {
        decl_list = new BList<Declaration>();
        decl_list->push_back(decls->function_declaration);
    } else if (decls->genvar_declaration != nullptr) {
        decl_list = decls->genvar_declaration;
    } else if (decls->integer_declaration != nullptr) {
        decl_list = decls->integer_declaration;
    } else if (decls->net_declaration != nullptr) {
        decl_list = decls->net_declaration;
    } else if (decls->real_declaration != nullptr) {
        decl_list = decls->real_declaration;
    } else if (decls->realtime_declaration != nullptr) {
        decl_list = decls->realtime_declaration;
    } else if (decls->reg_declaration != nullptr) {
        decl_list = decls->reg_declaration;
    } else if (decls->branch_declaration != nullptr) {
        decl_list = decls->branch_declaration;
    } else if (decls->task_declaration != nullptr) {
        decl_list = new BList<Declaration>();
        decl_list->push_back(decls->task_declaration);
    } else if (decls->time_declaration != nullptr) {
        decl_list = decls->time_declaration;
    } else if (decls->generate_declaration != nullptr) {
        contents_o->generates.merge(*decls->generate_declaration);
        delete decls->generate_declaration;
    } else {
        messageDebugAssert(false, "Unexpected case", nullptr, _sem);
    }

    if (decl_list == nullptr) {
        delete decls;
        return;
    }

    // Skip declarations with the same name of ports declared in the Entity
    bool found = false;
    for (BList<Declaration>::iterator i = decl_list->begin(); i != decl_list->end();) {
        Declaration *decl = *i;
        i                 = i.remove();

        found = false;
        if (iface_o != nullptr) {
            for (BList<Port>::iterator j = iface_o->ports.begin(); j != iface_o->ports.end(); ++j) {
                if (decl->getName() == (*j)->getName()) {
                    found = true;
                    break;
                }
            }
        }
        if (found) {
            auto *sig = dynamic_cast<Signal *>(decl);
            if (sig != nullptr && sig->getValue() != nullptr) {
                auto *assign = new Assign();
                assign->setLeftHandSide(new Identifier(decl->getName()));
                assign->setRightHandSide(sig->setValue(nullptr));

                contents_o->getGlobalAction()->actions.push_back(assign);
            }
            delete decl;
        } else {
            contents_o->declarations.push_back(decl);
        }
    }

    delete decl_list;
    delete decls;
}

auto VerilogParser::_composeAmsType(Type *portType, Type *declarationType) -> Type *
{
    // WARNING: TODO check this!
    if (portType == nullptr) {
        return hif::copy(declarationType);
    }

    if (dynamic_cast<TypeReference *>(declarationType) == nullptr) {
        // Nothing to do for RTL types: only AMS types must be composed.
        // E.g.:
        // input [5:0] in;
        // logic in;
        // must became: array of logic
        delete portType;
        return declarationType;
    }
    if (dynamic_cast<Bit *>(portType) != nullptr) {
        delete portType;
        return declarationType;
    }
    if (dynamic_cast<Bitvector *>(portType) != nullptr) {
        auto *t = dynamic_cast<Bitvector *>(portType);
        auto *a = new Array();
        a->setType(declarationType);
        a->setSigned(t->isSigned());
        a->setSpan(t->setSpan(nullptr));

        delete portType;
        return a;
    }
    if (dynamic_cast<Array *>(portType) != nullptr) {
        auto *t = dynamic_cast<Array *>(portType);
        auto *a = new Array();
        a->setType(_composeAmsType(t->getType(), declarationType));
        a->setSigned(t->isSigned());
        a->setSpan(t->setSpan(nullptr));

        delete portType;
        return a;
    }
    if (dynamic_cast<TypeReference *>(portType) != nullptr) {
        auto *portRef = dynamic_cast<TypeReference *>(portType);
        auto *declRef = dynamic_cast<TypeReference *>(declarationType);
        if (portRef->getName() == declRef->getName()) {
            messageAssert(portRef->getName() != std::string("ground"), "Unresolved ground type", nullptr, nullptr);
            delete portType;
            return declarationType;
        } if (portRef->getName() == std::string("ground")) {
            portRef->templateParameterAssigns.push_back(_factory.templateTypeArgument("T", declRef));
            return portRef;
        } else if (declRef->getName() == std::string("ground")) {
            declRef->templateParameterAssigns.push_back(_factory.templateTypeArgument("T", portRef));
            return declRef;
        } else {
            // mismatched and unknown...
        }
    }

    messageError("Unexpected port type", portType, _sem);
}

auto VerilogParser::_makeValueFromFilter(analog_filter_function_arg_t *arg) -> Value *
{
    if (arg == nullptr) {
        return nullptr;
    }
    messageAssert(
        (arg->identifier != nullptr) ^ (arg->constant_optional_arrayinit != nullptr), "unexpected parser result.",
        nullptr, nullptr);
    if (arg->identifier != nullptr) {
        Identifier *id = arg->identifier;
        delete arg;
        return id;
    }

    auto *agg         = new Aggregate();
    long long int ind = 0;
    for (BList<Value>::iterator i = arg->constant_optional_arrayinit->begin();
         i != arg->constant_optional_arrayinit->end(); ++ind) {
        Value *v = *i;
        i.remove();
        auto *alt = new AggregateAlt();
        alt->setValue(v);
        alt->indices.push_back(_factory.intval(ind));
        agg->alts.push_back(alt);
    }

    delete arg->constant_optional_arrayinit;
    delete arg;

    return agg;
}

auto VerilogParser::buildSystemObject() -> System *
{
    auto *system_o = new System();
    system_o->setName("system");

    system_o->designUnits.merge(*_designUnits);

    delete _designUnits;
    return system_o;
}

void VerilogParser::setVerilogAms(const bool b) { _isVerilogAms = b; }

auto VerilogParser::isVerilogAms() -> bool { return _isVerilogAms; }

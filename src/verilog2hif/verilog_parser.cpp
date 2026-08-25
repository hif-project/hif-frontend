/// @file verilog_parser.cpp
/// @brief
/// Copyright (c) 2024-2025, Electronic Systems Design (ESD) Group,
/// Univeristy of Verona.
/// This file is distributed under the BSD 2-Clause License.
/// See LICENSE.md for details.

#if (defined _MSC_VER)
#else
#pragma GCC diagnostic ignored "-Wredundant-decls"
#endif

#include <algorithm>
#include <cmath>

#include "verilog2hif/support.hpp"
#include "verilog2hif/verilog_parser.hpp"

using namespace hif;
using std::list;
using std::string;

BList<DesignUnit> *VerilogParser::_designUnits = new BList<DesignUnit>();

extern FILE *yyin;  // defined in verilogParser.cc
extern FILE *yyout; // defined in verilogParser.cc
extern auto init_buffer(const char *fname) -> bool;

// Bison forward declarations.
auto yylex_destroy() -> int;
auto yyparse(VerilogParser *parser) -> int;

VerilogParser::VerilogParser(const string &fileName, const Verilog2hifParseLine &cLine)
    : _fileName(fileName)
    , _tmpCustomLineNumber(0)
    , _tmpCustomColumnNumber(0)
    , _parseOnly(false)
    , _unit(nullptr)
    , _precision(nullptr)
    , _sem(hif::semantics::VerilogSemantics::getInstance())
    , _factory(_sem)
    , _cLine(cLine)
{
    yyfilename = fileName;
}

VerilogParser::~VerilogParser()
{
    delete _unit;
    delete _precision;
    _unit      = nullptr;
    _precision = nullptr;
}

auto VerilogParser::parse(bool parseOnly) -> bool
{
    _parseOnly = parseOnly;

    if (!init_buffer(_fileName.c_str())) {
        return false;
    }

    // Parser entry-point
    yyparse(this);
    // Destroy of lexer. Not called by Bison.
    yylex_destroy();

    return true;
}

auto VerilogParser::isParseOnly() const -> bool { return _parseOnly; }

void VerilogParser::setCodeInfo(Object *o, bool recursive)
{
    if (o == nullptr) {
        return;
    }

    o->setSourceFileName(_fileName);
    o->setSourceLineNumber(yylineno);
    o->setSourceColumnNumber(yycolumno);

    if (recursive) {
        _factory.codeInfo(o, o->getSourceFileName(), o->getSourceLineNumber(), o->getSourceColumnNumber());
    }
}

void VerilogParser::setCodeInfo(Object *o, keyword_data_t &keyword)
{
    if (o == nullptr) {
        return;
    }

    o->setSourceFileName(_fileName);
    o->setSourceLineNumber(keyword.line);
    o->setSourceColumnNumber(keyword.column);
}

void VerilogParser::setCurrentBlockCodeInfo(keyword_data_t &keyword)
{
    _tmpCustomLineNumber   = keyword.line;
    _tmpCustomColumnNumber = keyword.column;
}

void VerilogParser::setCurrentBlockCodeInfo(Object *other)
{
    if (other == nullptr) {
        return;
    }

    _tmpCustomLineNumber   = other->getSourceLineNumber();
    _tmpCustomColumnNumber = other->getSourceColumnNumber();
}

void VerilogParser::setCodeInfoFromCurrentBlock(Object *o)
{
    if (o == nullptr) {
        return;
    }

    o->setSourceFileName(_fileName);
    o->setSourceLineNumber(_tmpCustomLineNumber);
    o->setSourceColumnNumber(_tmpCustomColumnNumber);
}

///////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////

auto VerilogParser::parse_Identifier(char *identifier) -> Identifier *
{
    auto *ret = new Identifier();
    setCodeInfo(ret);

    ret->setName(identifier);

    free(identifier);
    return ret;
}

auto VerilogParser::parse_ModuleDeclarationStart(char *identifier) -> DesignUnit *
{
    auto *designUnit_o = new DesignUnit();
    View *view_o       = new View();

    // Setting eventual timescale
    if (_unit != nullptr && _precision != nullptr) {
        view_o->declarations.push_back(
            _factory.constant(_factory.time(), "hif_verilog_timescale_unit", hif::copy(_unit)));

        view_o->declarations.push_back(
            _factory.constant(_factory.time(), "hif_verilog_timescale_precision", hif::copy(_precision)));
    }

    auto *iface_o    = new Entity();
    auto *contents_o = new Contents();

    setCodeInfoFromCurrentBlock(designUnit_o);
    setCodeInfoFromCurrentBlock(view_o);
    setCodeInfoFromCurrentBlock(iface_o);
    setCodeInfoFromCurrentBlock(contents_o);

    // set the interface, the name and the language of the View
    view_o->setEntity(iface_o);
    view_o->setName("behav");
    view_o->setLanguageID(hif::rtl);

    auto *globact_o = new GlobalAction();
    setCodeInfoFromCurrentBlock(designUnit_o);
    contents_o->setGlobalAction(globact_o);

    view_o->setContents(contents_o);
    designUnit_o->views.push_back(view_o);
    designUnit_o->setName(identifier);

    free(identifier);

    return designUnit_o;
}

#ifdef __GNUC__
#pragma GCC diagnostic ignored "-Wstrict-overflow"
#endif
void VerilogParser::parse_ModuleDeclaration(
    DesignUnit *designUnit,
    hif::BList<hif::Declaration> *paramList,
    BList<Port> *list_of_ports,
    bool referencePortList,
    list<module_item_t *> *module_item_list)
{
    messageAssert(designUnit != nullptr && !designUnit->views.empty(), "unexpected case", nullptr, nullptr);
    View *view_o        = designUnit->views.back();
    Entity *interface_o = view_o->getEntity();

    if (paramList != nullptr) {
        view_o->templateParameters.merge(*paramList);
        delete paramList;
    }

    Contents *contents_o = view_o->getContents();
    // for each module item
    for (auto &i : *module_item_list) {
        if (i->port_declaration_identifiers != nullptr) {
            BList<Declaration> *decl_list = blist_scast<Declaration>(i->port_declaration_identifiers);
            contents_o->declarations.merge(*decl_list);
            delete i->port_declaration_identifiers;
        } else if (i->non_port_module_item != nullptr) {
            non_port_module_item_t *item = i->non_port_module_item;
            if (item->module_or_generate_item != nullptr) {
                module_or_generate_item_t *mod_item = item->module_or_generate_item;
                _fillBaseContentsFromModuleOrGenerateItem(contents_o, mod_item, designUnit);
            } else if (item->parameter_declaration != nullptr) {
                BList<ValueTP> *decl_list =
                    &blist_scast<Declaration>(item->parameter_declaration)->toOtherBList<ValueTP>();
                view_o->templateParameters.merge(*decl_list);
                delete item->parameter_declaration;
            } else if (item->specify_block != nullptr) {
                std::list<specify_item_t *> *specifyItems = item->specify_block;

                // See 'specify_item' in verilog.yxx
                messageAssert(specifyItems->empty(), "Specify block is not supported", nullptr, nullptr);

                delete item->specify_block;
            }
            //            else if ( generate_region )
            //            else if ( specparameter_declaration )
            else {
                messageDebugAssert(false, "Unexpected case", nullptr, _sem);
            }

            delete item;
        }

        delete i;
    }

    interface_o->ports.merge(*list_of_ports);
    if (referencePortList) {
        // Remove declarations with the same name of a port
        BList<Declaration> *decl_list = &contents_o->declarations;
        for (BList<Declaration>::iterator it = decl_list->begin(); it != decl_list->end();) {
            Port *found = nullptr;
            for (BList<Port>::iterator j = interface_o->ports.begin(); j != interface_o->ports.end(); ++j) {
                if ((*it)->getName() == (*j)->getName()) {
                    found = *j;
                    break;
                }
            }

            if (found == nullptr) {
                ++it;
                continue;
            }

            Port *port_o = dynamic_cast<Port *>(*it);
            if (port_o == nullptr) {
                // declaration with same name: in verilog it still is a port!!
                auto *decl = dynamic_cast<DataDeclaration *>(*it);
                messageAssert(decl != nullptr, "Expected decl", *it, nullptr);
                it = it.remove();

                // Normally, Port and Decl types collide.
                // For Verilog AMS, we need to set Port type to, e.g., electrical
                found->setType(_composeAmsType(found->setType(nullptr), decl->setType(nullptr)));

                // Fixing initial values for ports and ports with reg declaration

                auto *sig = dynamic_cast<Signal *>(decl);
                if (sig != nullptr && sig->getValue() != nullptr && !sig->checkProperty(IS_VARIABLE_TYPE)) {
                    auto *assign = new Assign();
                    _factory.codeInfo(assign, sig->getSourceFileName(), sig->getSourceLineNumber());
                    assign->setLeftHandSide(new Identifier(decl->getName()));
                    assign->setRightHandSide(sig->setValue(nullptr));

                    contents_o->getGlobalAction()->actions.push_back(assign);
                }

                if (decl->getValue() != nullptr) {
                    found->setValue(decl->setValue(nullptr));
                }

                if (sig != nullptr && sig->checkProperty(IS_VARIABLE_TYPE) && found->getValue() == nullptr) {
                    found->addProperty(IS_VARIABLE_TYPE);
                }

                delete decl;
                continue;
            }
            if (found->getType() != nullptr) {
                port_o->setType(_composeAmsType(port_o->setType(nullptr), found->setType(nullptr)));
            }

            it = it.remove();
            found->replace(port_o);
            delete found;
        }
    }

    delete list_of_ports;
    delete module_item_list;

    _designUnits->push_back(designUnit);
}

void VerilogParser::parse_ModuleDeclaration(
    DesignUnit *du,
    BList<Declaration> *paramList,
    list<non_port_module_item_t *> *non_port_module_item_list)
{
    setCurrentBlockCodeInfo(du);

    View *view_o = du->views.back();

    Contents *contents_o = view_o->getContents();

    // for each module item
    for (auto *item : *non_port_module_item_list) {
        if (item->parameter_declaration != nullptr) {
            BList<ValueTP> *decl_list = &blist_scast<Declaration>(item->parameter_declaration)->toOtherBList<ValueTP>();
            view_o->templateParameters.merge(*decl_list);
            delete item->parameter_declaration;
        } else if (item->module_or_generate_item != nullptr) {
            module_or_generate_item_t *mod_item = item->module_or_generate_item;

            if (mod_item->initial_construct != nullptr) {
                // check assignments which initialize a declaration within the contents
                //_performDeclInitialization( mod_item->initial_construct, du );
                auto *stateTable_o = new StateTable();
                setCodeInfoFromCurrentBlock(stateTable_o);
                stateTable_o->setName(NameTable::getInstance()->getFreshName("initial_process")); // HERE!!!!
                stateTable_o->setFlavour(pf_initial);

                Contents *contentsOfInitialBlock = mod_item->initial_construct;

                auto *state_o = new State();
                setCodeInfoFromCurrentBlock(state_o);
                state_o->setName(stateTable_o->getName());
                state_o->actions.merge(contentsOfInitialBlock->getGlobalAction()->actions);

                stateTable_o->states.push_back(state_o);

                contents_o->stateTables.push_back(stateTable_o);
                contents_o->declarations.merge(contentsOfInitialBlock->declarations);
                delete contentsOfInitialBlock;
            } else if (mod_item->local_parameter_declaration != nullptr) {
                BList<Declaration> *decl_list = blist_scast<Declaration>(mod_item->local_parameter_declaration);
                contents_o->declarations.merge(*decl_list);
                delete mod_item->local_parameter_declaration;
            } else if (mod_item->module_or_generate_item_declaration != nullptr) {
                // all other declarations
                _manageModuleItemDeclarations(nullptr, du, mod_item->module_or_generate_item_declaration);
            } else if (mod_item->always_construct != nullptr) {
                contents_o->stateTables.push_back(mod_item->always_construct);
            } else if (mod_item->analog_construct != nullptr) {
                contents_o->stateTables.push_back(mod_item->analog_construct);
            } else if (mod_item->continuous_assign != nullptr) {
                BList<Action> *action_list = blist_scast<Action>(mod_item->continuous_assign);
                contents_o->getGlobalAction()->actions.merge(*action_list);
                delete mod_item->continuous_assign;
            } else if (mod_item->module_instantiation != nullptr) {
                contents_o->instances.merge(*mod_item->module_instantiation);
                delete mod_item->module_instantiation;
            } else {
                messageDebugAssert(false, "Unexpected case", nullptr, _sem);
            }
        } else if (item->specify_block != nullptr) {
            // TODO, skipped
            // see parse_SpecifyBlock() function
            // see 'non_port_module_item: specify_block' rule
        }

        delete item;
    }

    if (paramList != nullptr) {
        view_o->templateParameters.merge(*paramList);
        delete paramList;
    }

    delete non_port_module_item_list;

    _designUnits->push_back(du);
}

auto VerilogParser::parse_AnalogVariableAssignment(Value *lvalue, Value *expression) -> Assign *
{
    auto *ret = new Assign();
    setCodeInfo(ret);
    ret->setLeftHandSide(lvalue);
    ret->setRightHandSide(expression);

    return ret;
}

auto VerilogParser::parse_InitialConstruct(statement_t *statement) -> Contents *
{
    auto *contents = new Contents();
    contents->setGlobalAction(new GlobalAction());

    _buildActionList(statement, contents->getGlobalAction()->actions, &contents->declarations);

    delete statement;
    return contents;
}

auto VerilogParser::parse_BlockingAssignment(Value *variable_lvalue, Value *expression, Value *delay_or_event_control)
    -> Assign *
{
    if (variable_lvalue == nullptr || expression == nullptr) {
        return nullptr;
    }

    auto *assign_o = new Assign();
    setCodeInfo(assign_o);

    assign_o->setRightHandSide(expression);
    assign_o->setLeftHandSide(variable_lvalue);
    assign_o->setDelay(delay_or_event_control);

    return assign_o;
}

auto VerilogParser::parse_NonBlockingAssignment(
    Value *variable_lvalue,
    Value *expression,
    Value *delay_or_event_control) -> Assign *
{
    Assign *assign_o = parse_BlockingAssignment(variable_lvalue, expression, delay_or_event_control);
    assign_o->addProperty(NONBLOCKING_ASSIGNMENT);

    return assign_o;
}

auto VerilogParser::parse_AlwaysConstruct(statement_t *statement) -> StateTable *
{
    auto *stateTable_o = new StateTable();
    setCodeInfoFromCurrentBlock(stateTable_o);

    if (statement->blockName.empty()) {
        stateTable_o->setName(NameTable::getInstance()->getFreshName("process"));
    } else {
        stateTable_o->setName(statement->blockName);
    }

    auto *state_o = new State();
    setCodeInfoFromCurrentBlock(state_o);
    state_o->setName(stateTable_o->getName());

    bool allSignals = false;
    if (statement->procedural_timing_control != nullptr) {
        _buildSensitivityFromEventControl(
            statement->procedural_timing_control->event_control, stateTable_o->sensitivity, allSignals);

        if (allSignals) {
            stateTable_o->addProperty("ALL_SIGNALS");
        }

        delete statement->procedural_timing_control->event_control;
        delete statement->procedural_timing_control;
        statement->procedural_timing_control = nullptr;
    }

    // In 100% correct designs, this is useless.
    // Needed since in case of no reset and FSM with state
    // assigned  with blocking statement,
    // SystemC will execute one more cycle, switching 2 states.
    // Ref design: vis/b01
    if (allSignals || !stateTable_o->sensitivity.empty() || !stateTable_o->sensitivityPos.empty() ||
        !stateTable_o->sensitivityNeg.empty()) {
        //stateTable_o->setDontInitialize(true);
        stateTable_o->setDontInitialize(false);
    }

    BList<Action> actionList;
    _buildActionList(statement, actionList, &stateTable_o->declarations);
    state_o->actions.merge(actionList);

    stateTable_o->states.push_back(state_o);

    delete statement;

    // Fixing sensitivity:
    for (BList<Value>::iterator it = stateTable_o->sensitivity.begin(); it != stateTable_o->sensitivity.end();) {
        if ((*it)->checkProperty(PROPERTY_SENSITIVE_POS)) {
            (*it)->removeProperty(PROPERTY_SENSITIVE_POS);
            stateTable_o->sensitivityPos.push_back(hif::copy(*it));
            it = it.erase();
        } else if ((*it)->checkProperty(PROPERTY_SENSITIVE_NEG)) {
            (*it)->removeProperty(PROPERTY_SENSITIVE_NEG);
            stateTable_o->sensitivityNeg.push_back(hif::copy(*it));
            it = it.erase();
        } else {
            ++it;
        }
    }

    return stateTable_o;
}

auto VerilogParser::parse_AnalogConstruct(analog_statement_t *statement) -> StateTable *
{
    auto *stateTable_o = new StateTable();
    setCodeInfoFromCurrentBlock(stateTable_o);
    stateTable_o->setFlavour(hif::pf_analog);

    if (statement->blockName.empty()) {
        stateTable_o->setName(NameTable::getInstance()->getFreshName("process"));
    } else {
        stateTable_o->setName(statement->blockName);
    }

    //stateTable_o->setDontInitialize(true);
    stateTable_o->setDontInitialize(false);

    auto *state_o = new State();
    setCodeInfoFromCurrentBlock(state_o);
    state_o->setName(stateTable_o->getName());

    BList<Action> actionList;
    _buildActionListFromAnalogStatement(statement, actionList);
    state_o->actions.merge(actionList);

    stateTable_o->states.push_back(state_o);

    delete statement;
    return stateTable_o;
}

auto VerilogParser::parse_Assignment(hif::Value *lvalue, Value *expression) -> Assign *
{
    if (lvalue == nullptr || expression == nullptr) {
        delete lvalue;
        delete expression;
        return nullptr;
    }

    auto *assign_o = new Assign();
    setCodeInfo(assign_o);

    assign_o->setRightHandSide(expression);
    assign_o->setLeftHandSide(lvalue);

    return assign_o;
}

auto VerilogParser::parse_ContinuousAssign(Value *delay3_opt, BList<Assign> *list_of_net_assignments) -> BList<Assign> *
{
    if (delay3_opt == nullptr) {
        return list_of_net_assignments;
    }

    for (BList<Assign>::iterator i = list_of_net_assignments->begin(); i != list_of_net_assignments->end(); ++i) {
        Assign *ass = *i;
        ass->setDelay(hif::copy(delay3_opt));
    }

    delete delay3_opt;
    return list_of_net_assignments;
}

auto VerilogParser::parse_PortReference(char *identifier) -> Port *
{
    Port *port_o = new Port();
    setCodeInfo(port_o);
    port_o->setName(identifier);

    free(identifier);
    return port_o;
}

auto VerilogParser::parse_BranchDeclaration(
    hif::BList<Value> *branch_terminal_list,
    hif::BList<Identifier> *list_of_identifiers) -> BList<Declaration> *
{
    messageAssert(
        branch_terminal_list != nullptr && list_of_identifiers != nullptr &&
            (branch_terminal_list->size() == 1 || branch_terminal_list->size() == 2),
        "Unexpected parameters", nullptr, _sem);

    Alias alias;
    setCodeInfo(&alias);
    FunctionCall *f = nullptr;
    if (branch_terminal_list->size() == 1) {
        Value *v1 = branch_terminal_list->front();
        branch_terminal_list->removeAll();
        f = _factory.functionCall(
            "vams_branch", nullptr, _factory.noTemplateArguments(), _factory.parameterArgument("param1", v1));
    } else if (branch_terminal_list->size() == 2) {
        Value *v1 = branch_terminal_list->front();
        Value *v2 = branch_terminal_list->back();
        branch_terminal_list->removeAll();
        f = _factory.functionCall(
            "vams_branch", nullptr, _factory.noTemplateArguments(),
            (_factory.parameterArgument("param1", v1), _factory.parameterArgument("param2", v2)));
    }
    setCodeInfo(f);
    alias.setValue(f);

    auto *ret = new BList<Declaration>();
    for (BList<Identifier>::iterator i = list_of_identifiers->begin(); i != list_of_identifiers->end(); ++i) {
        Alias *tmp = hif::copy(&alias);
        tmp->setName((*i)->getName());
        ret->push_back(tmp);
    }

    delete branch_terminal_list;
    delete list_of_identifiers;
    return ret;
}

auto VerilogParser::parse_BranchTerminal(char *identifier, Value *range_expression) -> Member *
{
    auto *member_o = new Member();
    setCodeInfo(member_o);
    member_o->setPrefix(new Identifier(identifier));
    member_o->setIndex(range_expression);

    free(identifier);

    return member_o;
}

auto VerilogParser::parse_PortType(const discipline_and_modifiers_t *discipline_and_modifiers) -> Type *
{
    Identifier *discipline = discipline_and_modifiers->discipline_identifier;
    Range *range           = discipline_and_modifiers->range;
    const bool isSigned    = discipline_and_modifiers->k_signed;

    if (discipline == nullptr) {
        return getSemanticType(range, isSigned);
    }

    // A bare identifier where a port's type belongs is a name the lexer does
    // not know. Two things are spelled that way and the parser cannot tell them
    // apart, because deciding needs the whole description: a Verilog-AMS
    // discipline, and a SystemVerilog type such as `logic`, which is not a
    // lexer keyword either. So the name is recorded and the decision deferred
    // to FixDescription's _fixAMSDisciplines - the same place a *body*
    // declaration written with `logic` already ends up (hif-frontend#21). If
    // the name resolves to a discipline it stays one; if it is `logic` it
    // becomes a resolved four-state bit; anything else fails there, where the
    // whole description is available to say so.
    auto *typeRef = new TypeReference();
    typeRef->setName(discipline->getName());
    setCodeInfo(typeRef);

    if (range == nullptr) {
        return typeRef;
    }

    // `logic [N:0]` is an Array of the element type, which visitArray folds
    // into the same Bitvector `reg [N:0]` produces (hif-frontend#21).
    auto *array = new Array();
    setCodeInfo(array);
    array->setSpan(hif::copy(range));
    array->setType(typeRef);
    array->setSigned(isSigned);

    return array;
}

auto VerilogParser::parse_InoutDeclaration(discipline_and_modifiers_t *discipline_and_modifiers, Identifier *identifier)
    -> Port *
{
    Port *port_o = new Port();
    setCodeInfo(port_o);

    port_o->setName(identifier->getName());
    port_o->setDirection(dir_inout);
    port_o->setType(parse_PortType(discipline_and_modifiers));

    delete discipline_and_modifiers->range;
    delete discipline_and_modifiers->discipline_identifier;
    delete discipline_and_modifiers;

    delete identifier;

    return port_o;
}

auto VerilogParser::parse_InoutDeclaration(
    discipline_and_modifiers_t *discipline_and_modifiers,
    BList<Identifier> *list_of_identifiers) -> BList<Port> *
{
    discipline_and_modifiers_t *d = nullptr;
    auto *ret                     = new BList<Port>();

    // for each identifier
    for (BList<Identifier>::iterator i = list_of_identifiers->begin(); i != list_of_identifiers->end();) {
        Identifier *id = *i;
        i              = i.remove();

        d = new discipline_and_modifiers_t(*discipline_and_modifiers);
        ret->push_back(parse_InoutDeclaration(d, id));
    }

    delete discipline_and_modifiers->range;
    delete discipline_and_modifiers->discipline_identifier;
    delete discipline_and_modifiers;

    delete list_of_identifiers;
    return ret;
}

auto VerilogParser::parse_InputDeclaration(discipline_and_modifiers_t *discipline_and_modifiers, Identifier *identifier)
    -> Port *
{
    Port *port = new Port();
    setCodeInfo(port, false);

    port->setName(identifier->getName());
    port->setDirection(dir_in);

    if (discipline_and_modifiers->discipline_identifier != nullptr) {
        // parse_PortType copies the range, so this branch owns the original -
        // unlike the two below, which transfer or ignore it.
        port->setType(parse_PortType(discipline_and_modifiers));
        delete discipline_and_modifiers->range;
    } else if (discipline_and_modifiers->range == nullptr) {
        port->setType(makeVerilogBitType());
        if (discipline_and_modifiers->k_signed) {
            yywarning("Signed directive is ignored.");
        }
    } else {
        auto *arr = new Bitvector();
        if (discipline_and_modifiers->range != nullptr) {
            arr->setSpan(discipline_and_modifiers->range);
        }
        arr->setLogic(true);
        arr->setResolved(true);
        arr->setSigned(discipline_and_modifiers->k_signed);
        port->setType(arr);
    }

    delete discipline_and_modifiers->discipline_identifier;
    delete discipline_and_modifiers;
    delete identifier;

    return port;
}

auto VerilogParser::parse_InputDeclaration(
    discipline_and_modifiers_t *discipline_and_modifiers,
    BList<Identifier> *list_of_identifiers) -> BList<Port> *
{
    auto *ret                     = new BList<Port>();
    discipline_and_modifiers_t *d = nullptr;

    // for each identifier
    for (BList<Identifier>::iterator i = list_of_identifiers->begin(); i != list_of_identifiers->end();) {
        Identifier *id = *i;
        i              = i.remove();

        d = new discipline_and_modifiers_t(*discipline_and_modifiers);
        ret->push_back(parse_InputDeclaration(d, id));
    }

    delete discipline_and_modifiers->range;
    delete discipline_and_modifiers->discipline_identifier;
    delete discipline_and_modifiers;

    delete list_of_identifiers;
    return ret;
}

auto VerilogParser::parse_OutputDeclaration(
    discipline_and_modifiers_t *discipline_and_modifiers,
    Identifier *identifier) -> Port *
{
    Port *port = new Port();
    setCodeInfo(port);

    port->setName(identifier->getName());
    port->setDirection(dir_out);
    port->setType(parse_PortType(discipline_and_modifiers));

    delete discipline_and_modifiers->range;
    // Was leaked here. Unreachable while a discipline identifier was refused
    // outright; reachable now, so it has to be released like the range.
    delete discipline_and_modifiers->discipline_identifier;
    delete discipline_and_modifiers;
    delete identifier;

    return port;
}

auto VerilogParser::parse_OutputDeclaration(
    bool k_signed,
    Range *range,
    char *identifier,
    Value *initVal,
    bool isReg) -> Port *
{
    Port *port_o = new Port();
    setCodeInfo(port_o);

    port_o->setName(identifier);
    port_o->setDirection(dir_out);
    port_o->setType(getSemanticType(range, k_signed));
    port_o->setValue(initVal);

    if (isReg && port_o->getValue() == nullptr) {
        port_o->addProperty(IS_VARIABLE_TYPE);
    }

    free(identifier);
    delete range;
    return port_o;
}

auto VerilogParser::parse_OutputDeclaration(Type *output_variable_type, char *identifier, Value *initVal) -> Port *
{
    Port *port_o = new Port();
    setCodeInfo(port_o);

    port_o->setName(identifier);
    port_o->setDirection(dir_out);
    port_o->setType(output_variable_type);
    port_o->setValue(initVal);

    free(identifier);
    return port_o;
}

auto VerilogParser::parse_OutputDeclaration(
    discipline_and_modifiers_t *discipline_and_modifiers,
    BList<Identifier> *list_of_identifiers) -> BList<Port> *
{
    auto *ret                     = new BList<Port>();
    discipline_and_modifiers_t *d = nullptr;

    // for each identifier
    for (BList<Identifier>::iterator i = list_of_identifiers->begin(); i != list_of_identifiers->end();) {
        Identifier *id = *i;
        i              = i.remove();

        d = new discipline_and_modifiers_t(*discipline_and_modifiers);
        ret->push_back(parse_OutputDeclaration(d, id));
    }

    delete discipline_and_modifiers->range;
    delete discipline_and_modifiers;
    delete list_of_identifiers;

    return ret;
}

auto VerilogParser::parse_OutputDeclaration(Type *output_variable_type, BList<Port> *list_of_variable_port_identifiers)
    -> BList<Port> *
{
    // for each identifier
    for (BList<Port>::iterator i = list_of_variable_port_identifiers->begin();
         i != list_of_variable_port_identifiers->end(); ++i) {
        (*i)->setDirection(dir_out);
        (*i)->setType(output_variable_type);

        auto *constValue_o = dynamic_cast<ConstValue *>((*i)->getValue());
        if (constValue_o != nullptr) {
            constValue_o = hif::manipulation::transformConstant(constValue_o, (*i)->getType());

            if (constValue_o == nullptr) {
                constValue_o = dynamic_cast<ConstValue *>((*i)->getValue());
            }
        }

        (*i)->setValue(constValue_o);
    }

    return list_of_variable_port_identifiers;
}

auto VerilogParser::parse_OutputDeclaration(
    bool k_signed,
    Range *range,
    BList<Port> *list_of_variable_port_identifiers,
    bool isReg) -> BList<Port> *
{
    // for each identifier
    for (BList<Port>::iterator i = list_of_variable_port_identifiers->begin();
         i != list_of_variable_port_identifiers->end(); ++i) {
        (*i)->setDirection(dir_out);
        (*i)->setType(getSemanticType(range, k_signed));

        auto *constValue_o = dynamic_cast<ConstValue *>((*i)->getValue());
        if (constValue_o != nullptr) {
            constValue_o = hif::manipulation::transformConstant(constValue_o, (*i)->getType());

            if (constValue_o == nullptr) {
                constValue_o = dynamic_cast<ConstValue *>((*i)->getValue());
            }
        }

        (*i)->setValue(constValue_o);

        if (isReg && (*i)->getValue() == nullptr) {
            (*i)->addProperty(IS_VARIABLE_TYPE);
        }
    }

    delete range;
    return list_of_variable_port_identifiers;
}

auto VerilogParser::parse_VariablePortIdentifier(char *identifier, Value *initVal) -> Port *
{
    Port *port_o = new Port();
    setCodeInfo(port_o);
    port_o->setName(identifier);
    port_o->setValue(initVal);

    free(identifier);
    return port_o;
}

auto VerilogParser::parse_EventIdentifier(char *identifier, hif::BList<Range> *dimension_list) -> Variable *
{
    Variable *v = nullptr;
    v           = new Variable();
    setCodeInfo(v);
    v->setName(identifier);

    if (dimension_list != nullptr && !dimension_list->empty()) {
        auto *array_prev = new Array();
        Range *range_o   = dimension_list->at(dimension_list->size() - 1);
        array_prev->setSpan(hif::copy(range_o));

        for (int i = dimension_list->size() - 1; i >= 0; i--) {
            range_o   = hif::copy(dimension_list->at(i));
            auto *tmp = new Array();
            tmp->setSpan(range_o);
            tmp->setType(array_prev);

            array_prev = tmp;
        }

        v->setType(array_prev);
    }

    free(identifier);
    delete dimension_list;
    return v;
}

auto VerilogParser::parse_LocalParameterDeclaration(
    bool K_signed_opt,
    Range *range_opt,
    BList<Assign> *list_of_param_assignments,
    hif::Type *parameter_type) -> BList<Const> *
{
    auto *valueTpList = parse_ParameterDeclaration(K_signed_opt, range_opt, list_of_param_assignments, parameter_type);
    auto *ret         = new BList<Const>();
    for (auto *vtp : *valueTpList) {
        auto *c = new Const();
        c->setName(vtp->getName());
        c->setCodeInfo(vtp->getCodeInfo());
        c->setType(vtp->getType());
        c->setValue(vtp->getValue());
        if (c->getType() == nullptr && c->getValue() != nullptr) {
            auto *cv = dynamic_cast<ConstValue *>(c->getValue());
            if (cv != nullptr) {
                c->setType(_sem->getTypeForConstant(cv));
            }
        }
        ret->push_back(c);
    }

    delete valueTpList;
    return ret;
}

auto VerilogParser::parse_ParameterDeclaration(
    bool k_signed,
    Range *range,
    BList<Assign> *list_of_param_assignments,
    hif::Type *parameter_type) -> BList<ValueTP> *
{
    auto *valuetp_list = new BList<ValueTP>();

    if (range == nullptr) {
        ValueTP *valuetp_o = nullptr;
        for (BList<Assign>::iterator it = list_of_param_assignments->begin(); it != list_of_param_assignments->end();) {
            Assign *par_o = *it;
            it            = it.remove();

            Value *prefix = hif::getTerminalPrefix(par_o->getLeftHandSide());
            auto *name    = dynamic_cast<Identifier *>(prefix);
            messageAssert(name != nullptr, "Unexpected prefix", prefix, _sem);

            valuetp_o = new ValueTP();
            setCodeInfo(valuetp_o);

            Type *t = hif::copy(parameter_type);
            if (k_signed) {
                hif::typeSetSigned(t, k_signed, _sem);
            }
            auto *tmp = dynamic_cast<Slice *>(par_o->getLeftHandSide());
            while (tmp != nullptr) {
                auto *a = new Array();
                a->setSpan(tmp->getSpan());
                a->setType(t);
                t   = a;
                tmp = dynamic_cast<Slice *>(tmp->getPrefix());
            }

            valuetp_o->setName(name->getName());
            valuetp_o->setValue(hif::copy(par_o->getRightHandSide()));
            valuetp_o->setType(t);
            valuetp_o->addProperty("signed", new BoolValue(k_signed));

            valuetp_list->push_back(valuetp_o);
            delete par_o;
        }
    } else {
        for (BList<Assign>::iterator it = list_of_param_assignments->begin(); it != list_of_param_assignments->end();) {
            Assign *par_o = *it;
            it            = it.remove();

            Value *prefix = hif::getTerminalPrefix(par_o->getLeftHandSide());
            auto *name    = dynamic_cast<Identifier *>(prefix);
            messageAssert(name != nullptr, "Unexpected prefix", prefix, _sem);

            auto *valuetp_o = new ValueTP();
            setCodeInfo(valuetp_o);
            valuetp_o->setName(name->getName());
            valuetp_o->setValue(hif::copy(par_o->getRightHandSide()));

            Type *t   = getSemanticType(range, k_signed);
            auto *tmp = dynamic_cast<Slice *>(par_o->getLeftHandSide());
            while (tmp != nullptr) {
                auto *a = new Array();
                a->setSpan(tmp->getSpan());
                a->setType(t);
                t   = a;
                tmp = dynamic_cast<Slice *>(tmp->getPrefix());
            }

            valuetp_o->setType(t);

            valuetp_list->push_back(valuetp_o);
            delete par_o;
        }
    }

    delete list_of_param_assignments;
    delete range;
    delete parameter_type;

    return valuetp_list;
}

auto VerilogParser::parse_ParamAssignment(
    char *identifier,
    Value *expr,
    Range *range_opt,
    hif::BList<Value *> *value_range_list_opt) -> Assign *
{
    messageAssert(value_range_list_opt == nullptr, "Unsupported value range list", nullptr, nullptr);

    auto *ret = new Assign();
    setCodeInfo(ret);

    Value *v = new Identifier(identifier);
    if (range_opt != nullptr) {
        auto *s = new Slice();
        s->setSpan(range_opt);
        s->setPrefix(v);
        v = s;
    }
    ret->setLeftHandSide(v);
    ret->setRightHandSide(expr);

    free(identifier);
    return ret;
}

auto VerilogParser::parse_Range(Value *lbound, Value *rbound) -> Range *
{
    //create a new rangeObject
    auto *ro = new Range();
    setCodeInfo(ro);

    ro->setLeftBound(lbound);
    ro->setRightBound(rbound);

    auto *constValueLeft = dynamic_cast<ConstValue *>(ro->getLeftBound());
    if (constValueLeft != nullptr) {
        Type *t = constValueLeft->getType();
        constValueLeft->setType(nullptr);
        delete t;
    }

    auto *constValueRight = dynamic_cast<ConstValue *>(ro->getRightBound());
    if (dynamic_cast<ConstValue *>(ro->getRightBound()) != nullptr) {
        Type *t = constValueRight->getType();
        constValueRight->setType(nullptr);
        delete t;
    }

    bool in1_expr     = false;
    bool in2_expr     = false;
    long long int in  = 0;
    long long int in2 = 0;

    if (dynamic_cast<IntValue *>(ro->getLeftBound()) != nullptr) {
        auto *io = dynamic_cast<IntValue *>(ro->getLeftBound());
        in       = io->getValue();
    } else {
        in1_expr = true;
    }

    if (dynamic_cast<IntValue *>(ro->getRightBound()) != nullptr) {
        auto *in2valob = dynamic_cast<IntValue *>(ro->getRightBound());
        in2            = in2valob->getValue();
    } else {
        in2_expr = true;
    }

    if (in1_expr && !in2_expr) {
        ro->setDirection(dir_downto);
    } else if (!in1_expr && in2_expr) {
        ro->setDirection(dir_upto);
    } else if (in1_expr && in2_expr) {
        ro->setDirection(dir_downto);
    } else if (in < in2) {
        ro->setDirection(dir_upto);
    } else {
        ro->setDirection(dir_downto);
    }

    return ro;
}

auto VerilogParser::parse_ExpressionUnaryOperator(Operator unary_op, Value *primary, bool negate) -> Value *
{
    if (primary == nullptr) {
        return nullptr;
    }

    auto *op = new Expression();
    setCodeInfo(op);

    op->setOperator(unary_op);
    op->setValue1(primary);

    if (negate) {
        op = new Expression(op_bnot, op);
        setCodeInfo(op);
    }

    return op;
}

auto VerilogParser::parse_ExpressionBinaryOperator(
    Value *expression1,
    Operator binary_op,
    Value *expression2,
    bool negate) -> Value *
{
    if (expression1 == nullptr || expression2 == nullptr) {
        return nullptr;
    }

    auto *expr = new Expression();
    setCodeInfo(expr);

    expr->setOperator(binary_op);
    expr->setValue1(expression1);
    expr->setValue2(expression2);

    if (negate) {
        expr = new Expression(op_bnot, expr);
        setCodeInfo(expr);
    }

    return expr;
}

auto VerilogParser::parse_ExpressionNorOperator(Value *primary) -> Value *
{
    if (primary == nullptr) {
        return nullptr;
    }

    auto *exprOr  = new Expression();
    auto *exprNot = new Expression();

    exprOr->setValue1(primary);
    exprOr->setOperator(op_orrd);

    exprNot->setValue1(exprOr);
    exprNot->setOperator(op_bnot);

    return exprNot;
}

auto VerilogParser::parse_ExpressionNandOperator(Value *primary) -> Value *
{
    if (primary == nullptr) {
        return nullptr;
    }

    auto *exprAnd = new Expression();
    auto *exprNot = new Expression();

    exprAnd->setValue1(primary);
    exprAnd->setOperator(op_andrd);

    exprNot->setValue1(exprAnd);
    exprNot->setOperator(op_bnot);

    return exprNot;
}

auto VerilogParser::parse_ExpressionTernaryOperator(Value *expression1, Value *expression2, Value *expression3)
    -> Value *
{
    if (expression1 == nullptr || expression2 == nullptr || expression3 == nullptr) {
        return nullptr;
    }

    When *when_o = new When();
    setCodeInfo(when_o);
    auto *whenAlt_o = new WhenAlt();
    setCodeInfo(whenAlt_o);

    whenAlt_o->setCondition(expression1);
    whenAlt_o->setValue(expression2);

    when_o->alts.push_back(whenAlt_o);
    when_o->setDefault(expression3);

    when_o->setLogicTernary(!_cLine.getTernary());

    return when_o;
}

auto VerilogParser::parse_RangeExpression(Value *lbound, Value *rbound) -> Value *
{
    auto *range_o = new Range();
    setCodeInfo(range_o);

    range_o->setLeftBound(lbound);
    range_o->setRightBound(rbound);

    // TODO: range direction must be fixed in postParsing
    range_o->setDirection(dir_downto);

    return range_o;
}

auto VerilogParser::parse_RangeExpressionPO_POS(Value *lbound, Value *rbound) -> Value *
{
    auto *range_o = new Range();
    setCodeInfo(range_o);

    range_o->setRightBound(lbound);
    range_o->setLeftBound(_factory.expression(
        _factory.expression(hif::copy(lbound), hif::op_plus, rbound), hif::op_minus, _factory.intval(1LL)));
    _factory.codeInfo(range_o, range_o->getSourceFileName(), range_o->getSourceLineNumber());
    // TODO: range direction must be fixed in postParsing
    range_o->setDirection(dir_downto);

    return range_o;
}

auto VerilogParser::parse_RangeExpressionPO_NEG(Value *lbound, Value *rbound) -> Value *
{
    auto *range_o = new Range();
    setCodeInfo(range_o);

    range_o->setLeftBound(lbound);
    range_o->setRightBound(_factory.expression(
        _factory.expression(hif::copy(lbound), hif::op_minus, rbound), hif::op_plus, _factory.intval(1LL)));
    _factory.codeInfo(range_o, range_o->getSourceFileName(), range_o->getSourceLineNumber());
    // TODO: range direction must be fixed in postParsing
    range_o->setDirection(dir_downto);

    return range_o;
}

auto VerilogParser::parse_EventTrigger(Value *hierarchical_identifier, hif::BList<Value> *bracket_expression_list)
    -> ValueStatement *
{
    messageAssert(hierarchical_identifier != nullptr, "Unexpected case", nullptr, nullptr);
    Expression *exprStatement = nullptr;
    exprStatement             = new Expression();
    // deref is the map of event trigger
    exprStatement->setOperator(op_deref);
    setCodeInfo(exprStatement);

    if (bracket_expression_list != nullptr && !bracket_expression_list->empty()) {
        Value *val    = bracket_expression_list->at(bracket_expression_list->size() - 1);
        auto *range_o = dynamic_cast<Range *>(val);

        Value *valuePrev = hierarchical_identifier;

        for (std::size_t i = 0; i < bracket_expression_list->size(); ++i) {
            if (range_o != nullptr) {
                // slice
                auto *s = new Slice();
                setCodeInfo(s);
                s->setSpan(hif::copy(range_o));
                s->setPrefix(valuePrev);
                valuePrev = s;
            } else {
                // member
                auto *s = new Member();
                setCodeInfo(s);
                s->setIndex(hif::copy(val));
                s->setPrefix(valuePrev);
                valuePrev = s;
            }
        }

        exprStatement->setValue1(valuePrev);
    } else {
        exprStatement->setValue1(hierarchical_identifier);
    }

    delete bracket_expression_list;

    ValueStatement *ret = nullptr;
    ret                 = new ValueStatement();
    ret->setValue(exprStatement);
    return ret;
}

auto VerilogParser::parse_PrimaryListOfMemberOrSlice(char *identifier, BList<Value> *range_expression_list) -> Value *
{
    Value *prefix = new Identifier(identifier);
    setCodeInfo(prefix);

    for (BList<Value>::iterator it = range_expression_list->begin(); it != range_expression_list->end();) {
        Value *value_o = *it;
        it             = it.remove();

        auto *range_o = dynamic_cast<Range *>(value_o);
        if (range_o != nullptr) {
            auto *slice_o = new Slice();
            setCodeInfo(slice_o);
            slice_o->setPrefix(prefix);
            slice_o->setSpan(range_o);
            prefix = slice_o;
        } else {
            auto *member_o = new Member();
            setCodeInfo(member_o);
            member_o->setPrefix(prefix);
            member_o->setIndex(value_o);
            prefix = member_o;
        }
    }

    free(identifier);
    delete range_expression_list;
    return prefix;
}

auto VerilogParser::parse_HierarchicalIdentifier(Value *hierarchical_identifier, Value *hierarchical_identifier_item)
    -> Value *
{
    auto *fieldRef_o = new FieldReference();
    auto *identifier = dynamic_cast<Identifier *>(hierarchical_identifier_item);

    messageAssert(identifier != nullptr, "Unexpected identifier", hierarchical_identifier_item, nullptr);

    fieldRef_o->setPrefix(hierarchical_identifier);
    fieldRef_o->setName(identifier->getName());

    delete hierarchical_identifier_item;

    setCodeInfo(fieldRef_o);
    return fieldRef_o;
}

auto VerilogParser::parse_HierarchicalIdentifierItem(char *identifier) -> Value *
{
    auto *ret = new Identifier(identifier);
    setCodeInfo(ret);

    free(identifier);
    return ret;
}

auto VerilogParser::parse_Concatenation(BList<Value> *value_list) -> Value *
{
    Value *ret = nullptr;

    ret = value_list->front();
    value_list->begin().remove();

    // start from second element
    for (BList<Value>::iterator i = value_list->begin(); i != value_list->end();) {
        Value *value_o = *i;
        i              = i.remove();

        auto *expr = new Expression();
        setCodeInfo(expr);
        expr->setValue1(ret);
        expr->setValue2(value_o);
        expr->setOperator(op_concat);

        ret = expr;
    }

    delete value_list;
    return ret;
}

auto VerilogParser::parse_MultipleConcatenation(Value *expression, Value *concatenation) -> Value *
{
    Value *ret = nullptr;

    auto *intExpression = dynamic_cast<IntValue *>(expression);
    if (intExpression != nullptr) {
        long long int t = intExpression->getValue();
        if (t < 0) {
            messageError("Negative index for concatenation", expression, nullptr);
        }

        BList<Value> ret_blist;
        for (int i = 0; i < t; ++i) {
            ret_blist.push_back(hif::copy(concatenation));
        }

        ret = concat(ret_blist);
        delete concatenation;
    } else {
        // The repetition value is a general expression.
        // Standard verilog requires that the repetition value can be statically
        // determined. In this case we cannot determine the size of the
        // operand, to set the two template parameters. It will be set by the
        // FixDescription after the parsing

        auto *fcall = new FunctionCall();
        setCodeInfo(fcall);
        fcall->setName("iterated_concat");
        auto *pp_expr = new ParameterAssign();
        setCodeInfo(pp_expr);
        pp_expr->setValue(concatenation);
        pp_expr->setName("expression");
        fcall->parameterAssigns.push_back(pp_expr);
        auto *tp_times = new ValueTPAssign();
        setCodeInfo(tp_times);
        tp_times->setValue(expression);
        tp_times->setName("times");
        fcall->templateParameterAssigns.push_back(tp_times);

        auto *lib = new Library();
        setCodeInfo(lib);
        lib->setName("standard");
        lib->setSystem(true);

        auto *inst = new Instance();
        setCodeInfo(inst);
        inst->setName("standard");
        inst->setReferencedType(lib);
        fcall->setInstance(inst);

        ret = fcall;
    }

    return ret;
}

auto VerilogParser::parse_ArrayInitialization(hif::BList<Value> *value_list) -> Value *
{
    auto *ret = new Aggregate();
    int i     = 0;
    while (!value_list->empty()) {
        auto *v = value_list->front();
        value_list->remove(v);
        auto *alt = new AggregateAlt();
        alt->setValue(v);
        alt->setCodeInfo(v->getCodeInfo());
        alt->indices.push_back(new IntValue(i));
        ++i;
        ret->alts.push_back(alt);
    }

    delete value_list;
    return ret;
}

auto VerilogParser::parse_RegDeclaration(
    discipline_identifier_signed_range_t *discipline_identifier_signed_range,
    BList<Signal> *list_of_variable_identifiers) -> BList<Declaration> *
{
    // ** VERILOG_AMS **
    messageAssert(
        discipline_identifier_signed_range->discipline_identifier == nullptr,
        "Discipline identifier is not supported (Verilog-AMS)",
        discipline_identifier_signed_range->discipline_identifier, nullptr);

    auto *res = new BList<Declaration>();

    Range *range_opt  = discipline_identifier_signed_range->range;
    bool K_signed_opt = discipline_identifier_signed_range->k_signed;

    // set the type of all Declaration of the list
    for (BList<Signal>::iterator i = list_of_variable_identifiers->begin(); i != list_of_variable_identifiers->end();) {
        Signal *var_o = *i;
        i             = i.remove();

        auto *array_o = dynamic_cast<Array *>(var_o->getType());

        if (array_o != nullptr) {
            //Bitvector *array2 = dynamic_cast<Bitvector*>( getSemanticType( range_opt, K_signed_opt ) );

            // set the type of the array received from register_variable_list
            // as array of bit where range is set as range_opt
            array_o->setType(getSemanticType(range_opt, K_signed_opt));
            array_o->setSigned(false);
        } else {
            messageAssert(var_o->getType() == nullptr, "Wrong type", var_o, nullptr);
            var_o->setType(getSemanticType(range_opt, K_signed_opt));
        }

        res->push_back(var_o);
    }

    delete discipline_identifier_signed_range->discipline_identifier;
    delete discipline_identifier_signed_range->range;
    delete discipline_identifier_signed_range;

    delete list_of_variable_identifiers;

    return res;
}

auto VerilogParser::parse_TimeDeclaration(BList<Signal> *list_of_variable_identifiers) -> BList<Declaration> *
{
    auto *res = new BList<Declaration>();

    // set the type of all Declaration of the list
    for (BList<Signal>::iterator i = list_of_variable_identifiers->begin(); i != list_of_variable_identifiers->end();) {
        Signal *var_o = *i;
        i             = i.remove();

        var_o->setType(new Time());
        res->push_back(var_o);
    }

    delete list_of_variable_identifiers;

    return res;
}

auto VerilogParser::parse_IntegerDeclaration(BList<Signal> *list_of_variable_identifiers) -> BList<Declaration> *
{
    auto *res = new BList<Declaration>();

    //set the type of all Declaration of the list
    for (BList<Signal>::iterator i = list_of_variable_identifiers->begin(); i != list_of_variable_identifiers->end();) {
        Signal *var = *i;
        i           = i.remove();

        if (dynamic_cast<Array *>(var->getType()) != nullptr) {
            auto *array = dynamic_cast<Array *>(var->getType());
            array->setType(makeVerilogIntegerType());

            // set the type of the Variable: it's a Variable, whit the type
            // set as Array Of Bit
            var->setType(array);
        } else {
            // this Variable isn't an array so we set only the type,the type
            // is Int
            var->setType(makeVerilogIntegerType());
        }

        res->push_back(var);
    }

    delete list_of_variable_identifiers;
    return res;
}

auto VerilogParser::parse_NetDeclaration(
    bool is_signed,
    Range *range,
    std::list<net_ams_decl_identifier_assignment_t *> *identifiers_or_assign,
    Type *explicitType) -> BList<Declaration> *
{
    messageAssert(range == nullptr || explicitType == nullptr, "Unexpected case", nullptr, nullptr);
    auto *signal_list = new BList<Declaration>();

    Signal *signal_o                           = nullptr;
    net_ams_decl_identifier_assignment_t *curr = nullptr;

    for (auto &it : *identifiers_or_assign) {
        curr     = it;
        signal_o = new Signal();
        setCodeInfo(signal_o);

        hif::TerminalPrefixOptions topt;
        topt.recurseIntoMembers          = false;
        topt.recurseIntoFieldRefs        = false;
        topt.recurseIntoDerefExpressions = false;
        topt.recurseIntoSlices           = true;
        auto *id                         = dynamic_cast<Identifier *>(hif::getTerminalPrefix(curr->identifier, topt));
        messageAssert(id != nullptr, "Expected identifier", curr->identifier, nullptr);
        signal_o->setName(id->getName());

        auto *sliceRight = dynamic_cast<Slice *>(curr->identifier);
        while (sliceRight != nullptr) {
            Range *rightRange = sliceRight->setSpan(nullptr);
            if (curr->dimension_list == nullptr) {
                curr->dimension_list = new hif::BList<hif::Range>();
            }
            curr->dimension_list->push_front(rightRange);
            sliceRight = dynamic_cast<Slice *>(sliceRight->getPrefix());
        }

        if (curr->init_expression != nullptr) {
            signal_o->setValue(curr->init_expression);
        }

        if (curr->dimension_list != nullptr) {
            auto *array_prev = new Array();
            Range *range_o   = curr->dimension_list->at(curr->dimension_list->size() - 1);
            array_prev->setSpan(range_o);

            for (int i = curr->dimension_list->size() - 1; i >= 0; i--) {
                range_o   = curr->dimension_list->at(i);
                auto *tmp = new Array();
                tmp->setSpan(range_o);
                tmp->setType(array_prev);

                array_prev = tmp;
            }

            if (explicitType != nullptr) {
                array_prev->setType(hif::copy(explicitType));
            } else {
                array_prev->setType(getSemanticType(range));
            }
            signal_o->setType(array_prev);
        } else {
            if (explicitType != nullptr) {
                signal_o->setType(hif::copy(explicitType));
            } else {
                signal_o->setType(getSemanticType(range, is_signed));
            }
        }

        signal_list->push_back(signal_o);

        delete curr->dimension_list;
        delete curr->identifier;
        delete curr;
    }

    delete range;
    delete explicitType;
    delete identifiers_or_assign;

    return signal_list;
}

auto VerilogParser::parse_InitialOrFinalStep(const string &name, std::list<string> *string_list) -> hif::FunctionCall *
{
    auto *ret = new FunctionCall();
    setCodeInfo(ret);
    ret->setName(name);
    if (string_list != nullptr) {
        int n = 1;
        for (auto &i : *string_list) {
            auto *pa = new ParameterAssign();
            setCodeInfo(pa);
            StringValue *txt = _factory.stringval(i);
            setCodeInfo(txt);
            pa->setValue(txt);
            pa->setName(NameTable::getInstance()->registerName(name, n));
            ++n;
        }
    }

    delete string_list;

    return ret;
}

auto VerilogParser::parse_RealDeclaration(BList<Signal> *list_of_real_identifiers) -> BList<Declaration> *
{
    auto *res = new BList<Declaration>();

    for (BList<Signal>::iterator i = list_of_real_identifiers->begin(); i != list_of_real_identifiers->end();) {
        Signal *var_o = *i;
        i             = i.remove();

        Real *real_o = new Real();
        setCodeInfo(real_o);
        real_o->setSpan(new Range(63, 0));

        auto *array_o = dynamic_cast<Array *>(var_o->getType());
        if (array_o != nullptr) {
            array_o->setType(real_o);
            var_o->setType(array_o);
        } else {
            var_o->setType(real_o);
        }

        res->push_back(var_o);
    }

    delete list_of_real_identifiers;

    return res;
}

auto VerilogParser::parse_Type(char *identifier, BList<Range> *non_empty_dimension_list) -> Signal *
{
    auto *ret = new Signal();
    setCodeInfo(ret);
    ret->addProperty(IS_VARIABLE_TYPE);
    ret->setName(identifier);

    auto *array_prev = new Array();
    Range *range_o   = non_empty_dimension_list->at(non_empty_dimension_list->size() - 1);
    array_prev->setSpan(hif::copy(range_o));

    for (int i = non_empty_dimension_list->size() - 1; i >= 0; i--) {
        range_o   = non_empty_dimension_list->at(i);
        auto *tmp = new Array();
        tmp->setSpan(hif::copy(range_o));
        tmp->setType(array_prev);

        array_prev = tmp;
    }

    ret->setType(array_prev);

    free(identifier);
    delete non_empty_dimension_list;

    return ret;
}

auto VerilogParser::parse_Type(char *identifier, Value *expression) -> Signal *
{
    auto *signal_o = new Signal();
    setCodeInfo(signal_o);

    signal_o->addProperty(IS_VARIABLE_TYPE);
    signal_o->setName(identifier);

    if (expression != nullptr) {
        signal_o->setValue(expression);
    }

    free(identifier);
    return signal_o;
}

auto VerilogParser::parse_Type(char *identifier) -> Signal *
{
    auto *signal_o = new Signal();
    setCodeInfo(signal_o);

    signal_o->addProperty(IS_VARIABLE_TYPE);
    signal_o->setName(identifier);

    free(identifier);
    return signal_o;
}

auto VerilogParser::parse_EventDeclaration(hif::BList<Variable> *list_of_variable_identifiers)
    -> hif::BList<Declaration> *
{
    auto *res = new BList<Declaration>();

    //set the type of all Declaration of the list
    for (BList<Variable>::iterator i = list_of_variable_identifiers->begin();
         i != list_of_variable_identifiers->end();) {
        Variable *var = *i;
        i             = i.remove();

        Event *e = nullptr;
        e        = new Event();
        setCodeInfo(e);

        if (dynamic_cast<Array *>(var->getType()) != nullptr) {
            auto *array = dynamic_cast<Array *>(var->getType());
            array->setType(e);

            // set the type of the Variable: it's a Variable, whit the type
            // set as Array Of Bit
            var->setType(array);
        } else {
            // this Variable isn't an array so we set only the type,the type
            // is Int
            var->setType(e);
        }

        res->push_back(var);
    }

    delete list_of_variable_identifiers;
    return res;
}

auto VerilogParser::parse_FunctionCall(Value *hierarchical_identifier, BList<Value> *expression_list) -> Value *
{
    auto *ret = new FunctionCall();
    setCodeInfo(ret);

    auto *identifier = dynamic_cast<Identifier *>(hierarchical_identifier);
    messageAssert(identifier != nullptr, "FunctionCall: unexpected identifier", hierarchical_identifier, nullptr);

    if (identifier != nullptr) {
        ret->setName(identifier->getName());
    }

    if (expression_list == nullptr) {
        delete hierarchical_identifier;
        return ret;
    }

    for (BList<Value>::iterator it = expression_list->begin(); it != expression_list->end();) {
        Value *value_o = *it;
        it             = it.remove();

        auto *parameterAssign_o = new ParameterAssign();
        setCodeInfo(parameterAssign_o);

        parameterAssign_o->setValue(value_o);
        ret->parameterAssigns.push_back(parameterAssign_o);
    }

    delete hierarchical_identifier;
    delete expression_list;
    return ret;
}

auto VerilogParser::parse_NatureAttributeReference(
    Value *hierarchical_identifier,
    const string &fieldName,
    Value *nature_attribute_identifier) -> Value *
{
    auto *id = dynamic_cast<Identifier *>(nature_attribute_identifier);
    messageAssert(id != nullptr, "Unexpected object.", nature_attribute_identifier, _sem);

    auto *fr = new FieldReference();
    setCodeInfo(fr);
    fr->setPrefix(hierarchical_identifier);
    fr->setName(fieldName);
    auto *fr2 = new FieldReference();
    setCodeInfo(fr2);
    fr2->setPrefix(fr);
    fr2->setName(id->getName());
    delete id;

    return fr2;
}

auto VerilogParser::parse_AmsFlowOfPort(Value *hierarchical_identifier, Value *expression_list) -> Value *
{
    auto *blist = new BList<Value>();
    blist->push_back(expression_list);
    Value *val = parse_FunctionCall(hierarchical_identifier, blist);
    auto *fc   = dynamic_cast<FunctionCall *>(val);
    messageAssert(fc != nullptr, "Unexpected value", val, _sem);
    auto *ret = new FunctionCall();
    setCodeInfo(ret);
    ret->setName(fc->getName());
    fc->setName("vams_flow_of_port");
    auto *pa = new ParameterAssign();
    setCodeInfo(pa);
    pa->setName("param1");
    pa->setValue(fc);
    ret->parameterAssigns.push_back(pa);

    return ret;
}

auto VerilogParser::parse_CaseStatement(
    Value *expression,
    BList<SwitchAlt> *case_item_list,
    const hif::CaseSemantics caseSem) -> Switch *
{
    auto *s = new Switch();
    setCodeInfoFromCurrentBlock(s);
    s->setCondition(expression);

    messageAssert(case_item_list != nullptr, "Expected case item list", nullptr, _sem);

    for (BList<SwitchAlt>::iterator it = case_item_list->begin(); it != case_item_list->end();) {
        SwitchAlt *switchAlt_o = *it;
        it                     = it.remove();

        if (switchAlt_o->conditions.empty()) {
            s->defaults.merge(switchAlt_o->actions);
            delete switchAlt_o;
        } else {
            s->alts.push_back(switchAlt_o);
        }
    }

    s->setCaseSemantics(caseSem);

    delete case_item_list;
    return s;
}

auto VerilogParser::parse_CaseItem(BList<Value> *expression_list, statement_t *statement_or_null) -> SwitchAlt *
{
    auto *ret = new SwitchAlt();
    setCodeInfo(ret);

    if (expression_list != nullptr) {
        ret->conditions.merge(*expression_list);
        delete expression_list;
    }

    if (statement_or_null != nullptr) {
        BList<Action> actionList;
        _buildActionList(statement_or_null, actionList);
        ret->actions.merge(actionList);
    }

    delete statement_or_null;
    return ret;
}

auto VerilogParser::parse_AnalogCaseItem(
    hif::BList<Value> *expression_list,
    analog_statement_t *analog_statement_or_null) -> SwitchAlt *
{
    auto *ret = new SwitchAlt();
    setCodeInfo(ret);

    if (expression_list != nullptr) {
        ret->conditions.merge(*expression_list);
        delete expression_list;
    }

    _buildActionListFromAnalogStatement(analog_statement_or_null, ret->actions);

    delete analog_statement_or_null;
    return ret;
}

auto VerilogParser::parse_ConditionalStatement(If *ifStatement) -> If *
{
    if (ifStatement->defaults.size() != 1) {
        return ifStatement;
    }

    // if there is only one default and it is IF object
    If *act = dynamic_cast<If *>(ifStatement->defaults.front());
    if (act == nullptr) {
        return ifStatement;
    }

    // remove the default
    ifStatement->defaults.remove(act);

    act = parse_ConditionalStatement(act);

    // move the Alts and Defaults from the IF to the parent IF
    ifStatement->alts.merge(act->alts);
    ifStatement->defaults.merge(act->defaults);

    delete act;

    return ifStatement;
}

auto VerilogParser::parse_ConditionalStatement(
    Value *expression,
    statement_t *statement_or_null,
    statement_t *else_statement_or_null) -> If *
{
    If *ret = new If();
    setCodeInfoFromCurrentBlock(ret);
    auto *ifAlt_o = new IfAlt();
    setCodeInfoFromCurrentBlock(ifAlt_o);

    ifAlt_o->setCondition(expression);
    if (statement_or_null != nullptr) {
        BList<Action> actionList;
        _buildActionList(statement_or_null, actionList);
        ifAlt_o->actions.merge(actionList);
    }

    ret->alts.push_back(ifAlt_o);

    if (else_statement_or_null != nullptr) {
        BList<Action> actionList;
        _buildActionList(else_statement_or_null, actionList);
        ret->defaults.merge(actionList);
    }

    delete statement_or_null;
    delete else_statement_or_null;

    return ret;
}

auto VerilogParser::parse_AnalogConditionalStatement(
    Value *expression,
    analog_statement_t *analog_statement_or_null,
    analog_statement_t *else_analog_statement_or_null) -> If *
{
    If *ret = new If();
    setCodeInfoFromCurrentBlock(ret);
    auto *ifAlt_o = new IfAlt();
    setCodeInfoFromCurrentBlock(ifAlt_o);

    ifAlt_o->setCondition(expression);
    _buildActionListFromAnalogStatement(analog_statement_or_null, ifAlt_o->actions);

    ret->alts.push_back(ifAlt_o);
    _buildActionListFromAnalogStatement(else_analog_statement_or_null, ret->defaults);

    delete analog_statement_or_null;
    delete else_analog_statement_or_null;

    return ret;
}

auto VerilogParser::parse_ElseIfStatementOrNullList(
    BList<IfAlt> *elseif_statement_or_null_list,
    Value *expression,
    statement_t *statement_or_null) -> BList<IfAlt> *
{
    auto *ret = new IfAlt();
    setCodeInfo(ret);
    ret->setCondition(expression);

    BList<Action> actionList;
    _buildActionList(statement_or_null, actionList);
    ret->actions.merge(actionList);

    elseif_statement_or_null_list->push_back(ret);

    delete statement_or_null;
    return elseif_statement_or_null_list;
}

auto VerilogParser::parse_IfGenerateConstruct(
    Value *expression,
    generate_block_t *generate_block_or_null_if,
    generate_block_t *generate_block_or_null_else) -> BList<Generate> *
{
    messageAssert(expression != nullptr, "Expected expression", nullptr, _sem);
    auto *ret = new BList<Generate>();

    if (generate_block_or_null_if != nullptr) {
        auto *ifRet = new IfGenerate();
        ifRet->setName(generate_block_or_null_if->generate_block_identifier_opt);
        setCodeInfo(ifRet);
        ifRet->setCondition(expression);
        for (auto *mod_item : *generate_block_or_null_if->module_or_generate_item_list) {
            _fillBaseContentsFromModuleOrGenerateItem(ifRet, mod_item);
        }
        ret->push_back(ifRet);
    }

    if (generate_block_or_null_else != nullptr) {
        auto *elseRet = new IfGenerate();
        elseRet->setName(generate_block_or_null_else->generate_block_identifier_opt);
        setCodeInfo(elseRet);
        auto *notExpression = new Expression();
        notExpression->setOperator(op_not);
        notExpression->setValue1(hif::copy(expression));
        elseRet->setCondition(notExpression);
        for (auto *mod_item : *generate_block_or_null_else->module_or_generate_item_list) {
            _fillBaseContentsFromModuleOrGenerateItem(elseRet, mod_item);
        }
        ret->push_back(elseRet);
    }

    delete generate_block_or_null_if;
    delete generate_block_or_null_else;
    return ret;
}

auto VerilogParser::parse_LoopGenerateConstruct(
    Assign *genvar_initialization,
    Value *expression,
    Assign *genvar_iteration,
    generate_block_t *generate_block) -> BList<Generate> *
{
    auto *ret = new BList<Generate>();

    //    Identifier * lhs = dynamic_cast<Identifier *>(genvar_initialization->getLeftHandSide());
    //    messageAssert(lhs != nullptr, "Expected identifier", genvar_initialization->getLeftHandSide(), _sem);
    //    Variable * var = new Variable();
    //    setCodeInfo(var);
    //    var->setName(lhs->getName());
    //    var->setValue(genvar_initialization->setRightHandSide(nullptr));
    //    var->setType(_factory.bitvector(_factory.range(31, 0), true, true, false, true));
    //    delete genvar_initialization;

    auto *fg = new ForGenerate();
    //    fg->initDeclarations.push_back(var);
    fg->initValues.push_back(genvar_initialization);
    fg->setCondition(expression);
    fg->stepActions.push_back(genvar_iteration);
    if (generate_block != nullptr) {
        fg->setName(generate_block->generate_block_identifier_opt);
        for (auto *mod_item : *generate_block->module_or_generate_item_list) {
            _fillBaseContentsFromModuleOrGenerateItem(fg, mod_item);
        }
    }
    ret->push_back(fg);

    delete generate_block;
    return ret;
}

auto VerilogParser::parse_ProceduralTimingControlStatement(
    procedural_timing_control_t *procedural_timing_control,
    statement_t *stat_or_null) -> statement_t *
{
    if (stat_or_null == nullptr) {
        stat_or_null = new statement_t();
    }

    stat_or_null->procedural_timing_control = procedural_timing_control;

    return stat_or_null;
}

auto VerilogParser::parse_AnalogEventControlStatement(
    analog_event_control_t *event_control,
    analog_statement_t *stat_or_null) -> analog_statement_t *
{
    if (stat_or_null == nullptr) {
        stat_or_null = new analog_statement_t();
    }

    stat_or_null->event_control = event_control;

    hif::BList<Action> actionList;
    _buildActionListFromAnalogStatement(stat_or_null, actionList);
    delete stat_or_null;
    messageAssert(actionList.size() == 1, "Unexpected parsing result", nullptr, nullptr);
    auto *w = dynamic_cast<Wait *>(actionList.front());
    actionList.removeAll();
    messageAssert(w != nullptr, "Unexpected parsing result", nullptr, nullptr);

    stat_or_null                        = new analog_statement_t();
    stat_or_null->analog_loop_statement = w;

    return stat_or_null;
}

auto VerilogParser::parse_WaitStatement(hif::Value *expression, statement_t *statement_or_null) -> statement_t *
{
    auto *ret           = new statement_t();
    ret->wait_statement = new hif::Wait();
    ret->wait_statement->setCondition(expression);
    // TODO check Wait sensitivity

    BList<Action> actionList;
    _buildActionListFromStatement(statement_or_null, actionList);
    ret->wait_statement->actions.merge(actionList);
    delete statement_or_null;

    return ret;
}

auto VerilogParser::parseOrAnalogEventExpression(analog_event_expression_t *e1, analog_event_expression_t *e2)
    -> analog_event_expression_t *
{
    messageAssert(
        e1->or_analog_event_expression != nullptr && e2->or_analog_event_expression != nullptr, "Unexpected case",
        e1->getFirstObject(), _sem);

    e1->or_analog_event_expression->merge(*e2->or_analog_event_expression);

    delete e2->or_analog_event_expression;
    delete e2;
    return e1;
}

template <typename T> auto VerilogParser::parse_LoopStatementWhile(Value *expression, T *statement) -> While *
{
    auto *ret = new While();
    setCodeInfoFromCurrentBlock(ret);

    ret->setCondition(expression);

    BList<Action> actionsList;
    _buildActionList(statement, actionsList);
    ret->actions.merge(actionsList);

    delete statement;
    return ret;
}

template While *VerilogParser::parse_LoopStatementWhile<statement_t>(Value *, statement_t *);
template While *VerilogParser::parse_LoopStatementWhile<analog_statement_t>(Value *, analog_statement_t *);

auto VerilogParser::parse_LoopStatementForever(statement_t *statement) -> While *
{
    auto *while_o = new While();
    setCodeInfoFromCurrentBlock(while_o);
    while_o->setCondition(new IntValue(1));

    BList<Action> actionsList;
    _buildActionList(statement, actionsList);
    while_o->actions.merge(actionsList);

    delete statement;
    return while_o;
}

template <typename T>
auto VerilogParser::parse_LoopStatementFor(
    Assign *init_variable_assign,
    Value *expression,
    Assign *step_variable_assign,
    T *statement) -> For *
{
    For *ret = new For();
    setCodeInfoFromCurrentBlock(ret);

    BList<Action> actionsList;
    _buildActionList(statement, actionsList);
    ret->forActions.merge(actionsList);

    ret->stepActions.push_back(step_variable_assign);
    ret->setCondition(expression);
    ret->initValues.push_back(init_variable_assign);

    delete statement;
    return ret;
}

template For *VerilogParser::parse_LoopStatementFor<statement_t>(Assign *, Value *, Assign *, statement_t *);
template For *
VerilogParser::parse_LoopStatementFor<analog_statement_t>(Assign *, Value *, Assign *, analog_statement_t *);

template <typename T> auto VerilogParser::parse_LoopStatementRepeat(Value *expression, T *statement) -> For *
{
    hif::HifFactory factory;
    auto *initD   = new BList<DataDeclaration>();
    auto *initV   = new BList<Action>();
    auto *stepAct = new BList<Action>();

    std::string indexName = NameTable::getInstance()->getFreshName("index");
    IntValue *lBound      = factory.intval(1, makeVerilogIntegerType());
    auto *rBound          = dynamic_cast<IntValue *>(expression);

    initD->push_back(factory.variable(makeVerilogIntegerType(), indexName, lBound));

    Operator condOp;
    Operator stepOp;

    if (lBound->getValue() <= rBound->getValue()) {
        condOp = op_le;
        stepOp = op_plus;
    } else {
        condOp = op_ge;
        stepOp = op_minus;
    }

    auto *id = new Identifier(indexName);
    setCodeInfoFromCurrentBlock(id);
    Expression *cond = factory.expression(id, condOp, rBound);

    stepAct->push_back(factory.assignment(
        hif::copy(id), factory.expression(hif::copy(id), stepOp, factory.intval(1, makeVerilogIntegerType()))));

    For *ret = new For();
    setCodeInfoFromCurrentBlock(ret);

    BList<Action> actionsList;
    _buildActionList(statement, actionsList);
    ret->forActions.merge(actionsList);
    ret->stepActions.merge(*stepAct);
    ret->setCondition(cond);
    ret->initValues.merge(*initV);
    ret->initDeclarations.merge(*initD);

    delete stepAct;
    delete initV;
    delete initD;
    delete statement;
    return ret;
}

template For *VerilogParser::parse_LoopStatementRepeat<statement_t>(Value *, statement_t *);
template For *VerilogParser::parse_LoopStatementRepeat<analog_statement_t>(Value *, analog_statement_t *);

auto VerilogParser::parse_TaskDeclaration(
    bool isAutomatic,
    char *identifier,
    list<task_item_declaration_t *> *task_item_declaration_list,
    statement_t *statement_or_null) -> Procedure *
{
    Procedure *proc   = nullptr;
    proc              = new Procedure();
    auto *state_table = new StateTable();
    auto *state       = new State();

    setCodeInfo(proc);
    setCodeInfo(state_table);
    setCodeInfo(state);

    for (auto &i : *task_item_declaration_list) {
        if (i->tf_declaration != nullptr) {
            for (BList<Port>::iterator j = i->tf_declaration->begin(); j != i->tf_declaration->end(); ++j) {
                Port *port_o  = (*j);
                auto *param_o = new Parameter();
                setCodeInfo(param_o);

                param_o->setSourceLineNumber(port_o->getSourceLineNumber());
                param_o->setSourceFileName(port_o->getSourceFileName());

                param_o->setName(port_o->getName());
                param_o->setType(hif::copy(port_o->getType()));
                param_o->setDirection(port_o->getDirection());
                Value *init_val = port_o->getValue();
                if (init_val != nullptr) {
                    param_o->setValue(hif::copy(init_val));
                }

                proc->parameters.push_back(param_o);
            }
            delete i->tf_declaration;
        } else if (i->block_item_declaration != nullptr) {
            block_item_declaration_t *block_decl = i->block_item_declaration;
            if (block_decl->local_parameter_declaration != nullptr) {
                BList<Declaration> *decl_list = blist_scast<Declaration>(block_decl->local_parameter_declaration);

                state_table->declarations.merge(*decl_list);
                delete block_decl->local_parameter_declaration;
            } else if (block_decl->integer_variable_declaration != nullptr) {
                BList<Declaration> *decl_list = blist_scast<Declaration>(block_decl->integer_variable_declaration);

                state_table->declarations.merge(*decl_list);
                delete block_decl->integer_variable_declaration;
            } else if (block_decl->reg_variable_declaration != nullptr) {
                BList<Declaration> *decl_list = blist_scast<Declaration>(block_decl->reg_variable_declaration);

                state_table->declarations.merge(*decl_list);
                delete block_decl->reg_variable_declaration;
            } else if (block_decl->variable_declaration != nullptr) {
                BList<Declaration> *decl_list = blist_scast<Declaration>(block_decl->variable_declaration);

                state_table->declarations.merge(*decl_list);
                delete block_decl->variable_declaration;
            } else {
                messageDebugAssert(false, "Unexpected case", nullptr, _sem);
            }

            delete i->block_item_declaration;
        }
    }

    _buildActionList(statement_or_null, state->actions);
    state->setName(NameTable::getInstance()->registerName("task_state"));

    state_table->setName(identifier);
    state_table->states.push_back(state);

    proc->setName(identifier);
    proc->setStateTable(state_table);

    if (!isAutomatic) {
        proc->addProperty(PROPERTY_TASK_NOT_AUTOMATIC);
    }

    delete task_item_declaration_list;
    free(identifier);
    return proc;
}

auto VerilogParser::parse_TaskDeclaration(
    bool isAutomatic,
    char *identifier,
    BList<Port> *task_port_list,
    list<block_item_declaration_t *> *block_item_declaration_list,
    statement_t *statement_or_null) -> Procedure *
{
    auto *proc        = new Procedure();
    auto *state_table = new StateTable();
    auto *state       = new State();

    setCodeInfo(proc);
    setCodeInfo(state_table);
    setCodeInfo(state);

    // The arguments. Same Port-to-Parameter transfer as the non-ANSI overload
    // performs on each task_item_declaration: only where the ports come from
    // differs between the two forms, never what they mean.
    for (BList<Port>::iterator it(task_port_list->begin()); it != task_port_list->end();) {
        Port *port_o = *it;
        it           = it.remove();

        auto *param_o = new Parameter();
        setCodeInfo(param_o);

        param_o->setSourceLineNumber(port_o->getSourceLineNumber());
        param_o->setSourceFileName(port_o->getSourceFileName());

        param_o->setName(port_o->getName());
        param_o->setType(hif::copy(port_o->getType()));
        param_o->setDirection(port_o->getDirection());
        Value *init_val = port_o->getValue();
        if (init_val != nullptr) {
            param_o->setValue(hif::copy(init_val));
        }

        proc->parameters.push_back(param_o);
        delete port_o;
    }
    delete task_port_list;

    // The body's own declarations. In the non-ANSI form these arrive
    // interleaved with the arguments inside task_item_declaration; here the
    // grammar has already separated them, so they are their own list.
    for (auto *block_decl : *block_item_declaration_list) {
        if (block_decl->local_parameter_declaration != nullptr) {
            BList<Declaration> *decl_list = blist_scast<Declaration>(block_decl->local_parameter_declaration);
            state_table->declarations.merge(*decl_list);
            delete block_decl->local_parameter_declaration;
        } else if (block_decl->integer_variable_declaration != nullptr) {
            BList<Declaration> *decl_list = blist_scast<Declaration>(block_decl->integer_variable_declaration);
            state_table->declarations.merge(*decl_list);
            delete block_decl->integer_variable_declaration;
        } else if (block_decl->reg_variable_declaration != nullptr) {
            BList<Declaration> *decl_list = blist_scast<Declaration>(block_decl->reg_variable_declaration);
            state_table->declarations.merge(*decl_list);
            delete block_decl->reg_variable_declaration;
        } else if (block_decl->variable_declaration != nullptr) {
            BList<Declaration> *decl_list = blist_scast<Declaration>(block_decl->variable_declaration);
            state_table->declarations.merge(*decl_list);
            delete block_decl->variable_declaration;
        } else {
            messageDebugAssert(false, "Unexpected case", nullptr, _sem);
        }

        delete block_decl;
    }
    delete block_item_declaration_list;

    _buildActionList(statement_or_null, state->actions);
    state->setName(NameTable::getInstance()->registerName("task_state"));

    state_table->setName(identifier);
    state_table->states.push_back(state);

    proc->setName(identifier);
    proc->setStateTable(state_table);

    if (!isAutomatic) {
        proc->addProperty(PROPERTY_TASK_NOT_AUTOMATIC);
    }

    free(identifier);
    return proc;
}

auto VerilogParser::parse_BlockVariableType(char *identifier, BList<Range> *dimension_list) -> Signal *
{
    auto *ret = new Signal();
    setCodeInfo(ret);
    ret->addProperty(IS_VARIABLE_TYPE);

    ret->setName(NameTable::getInstance()->registerName(identifier));
    free(identifier);

    messageAssert(dimension_list != nullptr, "Expected dimention list", nullptr, _sem);
    if (dimension_list->empty()) {
        delete dimension_list;
        return ret;
    }

    // make a multi-dimensional array from the range list

    Array *type = nullptr;
    Array *tmp  = nullptr;
    Array *prev = nullptr;

    for (BList<Range>::iterator i = dimension_list->begin(); i != dimension_list->end();) {
        Range *range_o = *i;
        i              = i.remove();

        tmp = new Array();
        setCodeInfo(tmp);
        tmp->setSpan(range_o);
        tmp->setType(prev);

        prev = tmp;
        type = hif::copy(tmp);
    }

    ret->setType(type);

    delete dimension_list;
    return ret;
}

auto VerilogParser::parse_DisableStatement(Value *hierarchical_identifier) -> Break *
{
    Break *ret = nullptr;
    ret        = new Break();
    setCodeInfo(ret);

    auto *id = dynamic_cast<Identifier *>(hierarchical_identifier);
    messageAssert(id != nullptr, "Unexpected message", hierarchical_identifier, _sem);

    ret->setName(id->getName());
    delete id;

    return ret;
}

auto VerilogParser::parse_BlockItemDeclaration_Reg(
    bool signed_opt,
    Range *range_opt,
    BList<Signal> *list_of_block_variable_identifiers) -> BList<Signal> *
{
    // set the range and the type of all registers of the list
    for (BList<Signal>::iterator i = list_of_block_variable_identifiers->begin();
         i != list_of_block_variable_identifiers->end(); ++i) {
        auto *array_o = dynamic_cast<Array *>((*i)->getType());
        if (array_o != nullptr) {
            // create an array of bit with the range 'range_opt'
            //Bitvector *array2 = dynamic_cast<Bitvector*>( getSemanticType( range_opt, signed_opt ) );

            array_o->setType(getSemanticType(range_opt, signed_opt));
            array_o->setSigned(false);
        } else {
            // The current Signal is not an Array
            messageAssert((*i)->getType() == nullptr, "Wrong type", *i, nullptr);
            (*i)->setType(getSemanticType(range_opt, signed_opt));
        }
    }

    delete range_opt;
    return list_of_block_variable_identifiers;
}

auto VerilogParser::parse_BlockItemDeclaration_Integer(BList<Signal> *list_of_block_variable_identifiers)
    -> BList<Signal> *
{
    for (BList<Signal>::iterator i = list_of_block_variable_identifiers->begin();
         i != list_of_block_variable_identifiers->end(); ++i) {
        if ((*i)->getType() != nullptr && dynamic_cast<Array *>((*i)->getType()) != nullptr) {
            auto *array = dynamic_cast<Array *>((*i)->getType());
            // set the array type
            array->setType(makeVerilogIntegerType());
            (*i)->setType(array);
        } else {
            (*i)->setType(makeVerilogIntegerType());
        }
    }

    return list_of_block_variable_identifiers;
}

//Procedure *
//ParserCode::parse_TaskDeclaration( Identifier * identifier,
//        BList<Port> * task_port_list,
//        BList<block_item_declaration_t> * block_item_declaration_list,
//        BList<Action> * statements )
//{
//
//}

auto VerilogParser::parse_TfDeclaration(
    PortDirection dir,
    bool isSigned,
    Range *range_opt,
    Identifier *identifier,
    Type *task_port_type) -> Port *
{
    Port *port_o = new Port();
    setCodeInfo(port_o);

    port_o->setName(identifier->getName());
    port_o->setDirection(dir);

    if (task_port_type == nullptr) {
        port_o->setType(getSemanticType(range_opt, isSigned));
    } else {
        port_o->setType(hif::copy(task_port_type));
    }

    delete range_opt;
    return port_o;
}

auto VerilogParser::parse_TfDeclaration(
    PortDirection dir,
    bool isSigned,
    Range *range_opt,
    BList<Identifier> *list_of_identifiers,
    Type *task_port_type) -> BList<Port> *
{
    auto *ret = new BList<Port>();

    for (BList<Identifier>::iterator i = list_of_identifiers->begin(); i != list_of_identifiers->end(); ++i) {
        ret->push_back(parse_TfDeclaration(dir, isSigned, hif::copy(range_opt), *i, task_port_type));
    }

    delete range_opt;
    delete list_of_identifiers;
    delete task_port_type;

    return ret;
}

auto VerilogParser::parse_FunctionDeclaration(
    Type *function_range_or_type,
    char *identifier,
    list<function_item_declaration_t *> *function_item_declaration_list,
    statement_t *statements) -> Function *
{
    auto *function_o   = new Function();
    auto *stateTable_o = new StateTable();

    setCodeInfo(function_o);
    setCodeInfo(stateTable_o);

    stateTable_o->setName(NameTable::getInstance()->registerName(identifier));

    // process the list of declaration items
    for (auto *fun_item : *function_item_declaration_list) {
        if (fun_item->tf_input_declaration != nullptr) {
            BList<Port> *port_list = fun_item->tf_input_declaration;
            for (BList<Port>::iterator j = port_list->begin(); j != port_list->end();) {
                Port *port_o = *j;
                j            = j.remove();

                auto *param_o = new Parameter();
                setCodeInfo(param_o);

                param_o->setSourceLineNumber(port_o->getSourceLineNumber());
                param_o->setSourceFileName(port_o->getSourceFileName());

                param_o->setName(port_o->getName());
                param_o->setType(hif::copy(port_o->getType()));
                param_o->setDirection(port_o->getDirection());
                Value *init_val = port_o->getValue();
                if (init_val != nullptr) {
                    param_o->setValue(hif::copy(init_val));
                }

                function_o->parameters.push_back(param_o);
                delete port_o;
            }

            delete fun_item->tf_input_declaration;
        } else if (fun_item->block_item_declaration != nullptr) {
            block_item_declaration_t *block_decl = fun_item->block_item_declaration;

            if (block_decl->local_parameter_declaration != nullptr) {
                delete block_decl->local_parameter_declaration;
                messageDebugAssert(false, "Not implemented yet!", nullptr, _sem);
                // TODO
            } else if (block_decl->integer_variable_declaration != nullptr) {
                BList<Declaration> *decl_list = blist_scast<Declaration>(block_decl->integer_variable_declaration);
                stateTable_o->declarations.merge(*decl_list);
                delete block_decl->integer_variable_declaration;
            } else if (block_decl->reg_variable_declaration != nullptr) {
                BList<Declaration> *decl_list = blist_scast<Declaration>(block_decl->reg_variable_declaration);
                stateTable_o->declarations.merge(*decl_list);
                delete block_decl->reg_variable_declaration;
            } else if (block_decl->variable_declaration != nullptr) {
                BList<Declaration> *decl_list = blist_scast<Declaration>(block_decl->variable_declaration);
                stateTable_o->declarations.merge(*decl_list);
                delete block_decl->variable_declaration;
            } else {
                messageDebugAssert(false, "Unexpected case", nullptr, _sem);
            }

            delete fun_item->block_item_declaration;
        }

        delete fun_item;
    }

    auto *returnValue = new Variable();
    setCodeInfo(returnValue);
    returnValue->setName(NameTable::getInstance()->registerName(identifier));

    function_o->setName(NameTable::getInstance()->registerName(identifier));

    // the function has a returned type
    if (function_range_or_type != nullptr) {
        function_o->setType(function_range_or_type);
        returnValue->setType(hif::copy(function_range_or_type));
    } else {
        /* The use of a range_or_type shall be optional.
         * A function specified without a range or type defaults
         * to a one bit reg for the return value*/
        Bit *bo = makeVerilogBitType();

        function_o->setType(dynamic_cast<Type *>(hif::copy(bo)));
        returnValue->setType(dynamic_cast<Type *>(hif::copy(bo)));
        delete (bo);
    }
    stateTable_o->declarations.push_back(returnValue);

    auto *state = new State();
    setCodeInfo(state);

    state->setName(NameTable::getInstance()->registerName("function_state"));

    BList<Action> actionsList;
    _buildActionList(statements, actionsList);
    state->actions.merge(actionsList);

    auto *returnObj = new Return();
    setCodeInfo(returnObj);

    auto *nameVarReturn = new Identifier();
    setCodeInfo(nameVarReturn);

    nameVarReturn->setName(NameTable::getInstance()->registerName(identifier));
    returnObj->setValue(nameVarReturn);
    state->actions.push_back(returnObj);

    stateTable_o->states.push_back(state);
    function_o->setStateTable(stateTable_o);

    free(identifier);
    delete statements;
    delete function_item_declaration_list;
    return function_o;
}

auto VerilogParser::parse_FunctionDeclaration(
    Type *function_range_or_type,
    char *identifier,
    hif::BList<Port> *function_port_list,
    std::list<block_item_declaration_t *> *block_item_declaration_list,
    statement_t *statements) -> Function *
{
    auto *function_o  = new Function();
    auto *state_table = new StateTable();

    setCodeInfo(function_o);
    setCodeInfo(state_table);

    state_table->setName(NameTable::getInstance()->registerName(identifier));

    // Function port list
    for (BList<Port>::iterator it(function_port_list->begin()); it != function_port_list->end();) {
        Port *port_o = *it;
        it           = it.remove();

        auto *param_o = new Parameter();
        setCodeInfo(param_o);

        param_o->setSourceLineNumber(port_o->getSourceLineNumber());
        param_o->setSourceFileName(port_o->getSourceFileName());

        param_o->setName(port_o->getName());
        param_o->setType(hif::copy(port_o->getType()));
        param_o->setDirection(port_o->getDirection());
        Value *init_val = port_o->getValue();
        if (init_val != nullptr) {
            param_o->setValue(hif::copy(init_val));
        }

        function_o->parameters.push_back(param_o);
        delete port_o;
    }

    // process the list of declaration items
    for (auto *decl_item : *block_item_declaration_list) {
        if (decl_item->local_parameter_declaration != nullptr) {
            delete decl_item->local_parameter_declaration;
            messageDebugAssert(false, "Not implemented yet!", nullptr, _sem);
            // TODO
        } else if (decl_item->integer_variable_declaration != nullptr) {
            BList<Declaration> *decl_list = blist_scast<Declaration>(decl_item->integer_variable_declaration);
            state_table->declarations.merge(*decl_list);
            delete decl_item->integer_variable_declaration;
        } else if (decl_item->reg_variable_declaration != nullptr) {
            BList<Declaration> *decl_list = blist_scast<Declaration>(decl_item->reg_variable_declaration);
            state_table->declarations.merge(*decl_list);
            delete decl_item->reg_variable_declaration;
        } else if (decl_item->variable_declaration != nullptr) {
            BList<Declaration> *decl_list = blist_scast<Declaration>(decl_item->variable_declaration);
            state_table->declarations.merge(*decl_list);
            delete decl_item->variable_declaration;
        } else {
            messageDebugAssert(false, "Unexpected case", nullptr, _sem);
        }

        delete decl_item;
    }

    auto *returnValue = new Variable();
    setCodeInfo(returnValue);
    returnValue->setName(NameTable::getInstance()->registerName(identifier));

    function_o->setName(NameTable::getInstance()->registerName(identifier));

    // the function has a returned type
    if (function_range_or_type != nullptr) {
        function_o->setType(function_range_or_type);
        returnValue->setType(hif::copy(function_range_or_type));
    } else {
        /* The use of a range_or_type shall be optional.
         * A function specified without a range or type defaults
         * to a one bit reg for the return value*/
        Bit *bo = makeVerilogBitType();

        function_o->setType(dynamic_cast<Type *>(hif::copy(bo)));
        returnValue->setType(dynamic_cast<Type *>(hif::copy(bo)));
        delete (bo);
    }
    state_table->declarations.push_back(returnValue);

    auto *state = new State();
    setCodeInfo(state);

    state->setName(NameTable::getInstance()->registerName("function_state"));

    BList<Action> actionsList;
    _buildActionList(statements, actionsList);
    state->actions.merge(actionsList);

    auto *returnObj = new Return();
    setCodeInfo(returnObj);

    auto *nameVarReturn = new Identifier();
    setCodeInfo(nameVarReturn);

    nameVarReturn->setName(NameTable::getInstance()->registerName(identifier));
    returnObj->setValue(nameVarReturn);
    state->actions.push_back(returnObj);

    state_table->states.push_back(state);
    function_o->setStateTable(state_table);

    free(identifier);
    delete statements;
    delete function_port_list;
    return function_o;
}

auto VerilogParser::parse_BranchProbeFunctionCall(
    hif::Value *nature_attribute_identifier,
    hif::Value *hierarchical_identifier1,
    hif::Value *hierarchical_identifier2) -> hif::FunctionCall *
{
    messageAssert(hierarchical_identifier1 != nullptr, "Expected at least one parameter", nullptr, nullptr);

    auto *id = dynamic_cast<Identifier *>(nature_attribute_identifier);
    messageAssert(id != nullptr, "Expected identifier", nature_attribute_identifier, nullptr);

    auto *param1 = new ParameterAssign();
    setCodeInfo(param1);
    param1->setName("param1");
    param1->setValue(hierarchical_identifier1);

    auto *ret = new FunctionCall();
    setCodeInfo(ret);
    ret->setName(id->getName());
    ret->parameterAssigns.push_back(param1);

    if (hierarchical_identifier2 != nullptr) {
        auto *param2 = new ParameterAssign();
        setCodeInfo(param2);
        param2->setName("param2");
        param2->setValue(hierarchical_identifier2);

        ret->parameterAssigns.push_back(param2);
    }

    delete nature_attribute_identifier;
    return ret;
}

auto VerilogParser::parse_AnalogDifferentialFunctionCall(
    const char *function_name,
    Value *expression1,
    Value *expression2,
    Value *expression3,
    Value *expression4) -> FunctionCall *
{
    ParameterAssign *param1 = nullptr;
    if (expression1 != nullptr) {
        param1 = new ParameterAssign();
        setCodeInfo(param1);
        param1->setName("param1");
        param1->setValue(expression1);
    }

    ParameterAssign *param2 = nullptr;
    if (expression2 != nullptr) {
        param2 = new ParameterAssign();
        setCodeInfo(param2);
        param2->setName("param2");
        param2->setValue(expression2);
    }

    ParameterAssign *param3 = nullptr;
    if (expression3 != nullptr) {
        param3 = new ParameterAssign();
        setCodeInfo(param3);
        param3->setName("param3");
        param3->setValue(expression3);
    }

    ParameterAssign *param4 = nullptr;
    if (expression4 != nullptr) {
        param4 = new ParameterAssign();
        setCodeInfo(param4);
        param4->setName("param4");
        param4->setValue(expression4);
    }

    auto *ret = new FunctionCall();
    setCodeInfo(ret);
    ret->setName(function_name);
    if (expression1 != nullptr) {
        ret->parameterAssigns.push_back(param1);
    }
    if (expression2 != nullptr) {
        ret->parameterAssigns.push_back(param2);
    }
    if (expression3 != nullptr) {
        ret->parameterAssigns.push_back(param3);
    }
    if (expression4 != nullptr) {
        ret->parameterAssigns.push_back(param4);
    }

    return ret;
}

auto VerilogParser::parse_ContributionStatement(hif::Value *branch_probe_function_call, hif::Value *expression)
    -> hif::ProcedureCall *
{
    messageAssert(
        branch_probe_function_call != nullptr && expression != nullptr, "Expected two parameters", nullptr, nullptr);

    auto *param1 = new ParameterAssign();
    setCodeInfo(param1);
    param1->setName("param1");
    param1->setValue(branch_probe_function_call);

    auto *param2 = new ParameterAssign();
    setCodeInfo(param2);
    param2->setName("param2");
    param2->setValue(expression);

    auto *ret = new ProcedureCall(); // TODO prefix with library?
    setCodeInfo(ret);
    ret->setName("vams_contribution_statement");
    ret->parameterAssigns.push_back(param1);
    ret->parameterAssigns.push_back(param2);
    return ret;
}

auto VerilogParser::parse_IndirectContributionStatement(
    Value *branch_probe_function_call,
    Value *indirect_expression,
    Value *expression) -> ProcedureCall *
{
    messageAssert(
        branch_probe_function_call != nullptr && indirect_expression != nullptr && expression != nullptr,
        "Expected three parameters", nullptr, nullptr);

    auto *param1 = new ParameterAssign();
    setCodeInfo(param1);
    param1->setName("param1");
    param1->setValue(branch_probe_function_call);

    auto *param2 = new ParameterAssign();
    setCodeInfo(param2);
    param2->setName("param2");
    auto *e = new Expression();
    setCodeInfo(e);
    e->setOperator(hif::op_case_eq);
    e->setValue1(indirect_expression);
    e->setValue2(expression);
    param2->setValue(e);

    auto *ret = new ProcedureCall(); // TODO prefix with library?
    setCodeInfo(ret);
    ret->setName("vams_indirect_contribution_statement");
    ret->parameterAssigns.push_back(param1);
    ret->parameterAssigns.push_back(param2);
    return ret;
}

auto VerilogParser::parse_AnalogBuiltInFunctionCall(
    hif::Identifier *analog_built_in_function_name,
    hif::Value *analog_expression1,
    hif::Value *analog_expression2) -> Value *
{
    auto *param1 = new ParameterAssign();
    setCodeInfo(param1);
    param1->setName("param1");
    param1->setValue(analog_expression1);

    ParameterAssign *param2 = nullptr;
    if (analog_expression2 != nullptr) {
        param2 = new ParameterAssign();
        setCodeInfo(param2);
        param2->setName("param2");
        param2->setValue(analog_expression2);
    }

    FunctionCall *ret = nullptr;
    ret               = new FunctionCall();
    setCodeInfo(ret);
    ret->setName(analog_built_in_function_name->getName());
    ret->parameterAssigns.push_back(param1);
    if (analog_expression2 != nullptr) {
        ret->parameterAssigns.push_back(param2);
    }

    delete analog_built_in_function_name;

    // Now managing special cases:
    if (ret->getName() == "pow") {
        Expression *e = nullptr;
        e             = new Expression();
        e->setOperator(hif::op_pow);
        e->setValue1(ret->parameterAssigns.front()->setValue(nullptr));
        e->setValue2(ret->parameterAssigns.back()->setValue(nullptr));
        delete ret;
        return e;
    }
    if (ret->getName() == "sqrt") {
        Expression *e = nullptr;
        e             = new Expression();
        e->setOperator(hif::op_pow);
        e->setValue1(ret->parameterAssigns.front()->setValue(nullptr));
        e->setValue2(_factory.realval(0.5));
        delete ret;
        return e;
    }

    return ret;
}

auto VerilogParser::parse_analysisFunctionCall(std::list<string> *string_list) -> FunctionCall *
{
    messageAssert(
        string_list->size() <= 1, "vams method analysis() with multiple parameters is not supported yet.", nullptr,
        nullptr);

    FunctionCall *fc = nullptr;
    fc               = new FunctionCall();
    fc->setName("analysis");
    setCodeInfo(fc);
    for (auto &i : *string_list) {
        auto *s = new String();
        setCodeInfo(s);
        StringValue *t = nullptr;
        t              = new StringValue();
        setCodeInfo(t);
        t->setType(s);
        t->setValue(i);
        ParameterAssign *pa = nullptr;
        pa                  = new ParameterAssign();
        setCodeInfo(pa);
        pa->setName("param1");
        pa->setValue(t);
        fc->parameterAssigns.push_back(pa);
    }

    delete string_list;
    return fc;
}

auto VerilogParser::parse_AnalogFilterFunctionCall(
    const char *function_name,
    hif::Value *expression1,
    hif::Value *expression2,
    hif::Value *expression3,
    hif::Value *expression4,
    hif::Value *expression5,
    hif::Value *expression6) -> hif::FunctionCall *
{
    auto *param1 = new ParameterAssign();
    setCodeInfo(param1);
    param1->setName("param1");
    param1->setValue(expression1);

    ParameterAssign *param2 = nullptr;
    if (expression2 != nullptr) {
        param2 = new ParameterAssign();
        setCodeInfo(param2);
        param2->setName("param2");
        param2->setValue(expression2);
    }

    ParameterAssign *param3 = nullptr;
    if (expression3 != nullptr) {
        param3 = new ParameterAssign();
        setCodeInfo(param3);
        param3->setName("param3");
        param3->setValue(expression3);
    }

    ParameterAssign *param4 = nullptr;
    if (expression4 != nullptr) {
        param4 = new ParameterAssign();
        setCodeInfo(param4);
        param4->setName("param4");
        param4->setValue(expression4);
    }

    ParameterAssign *param5 = nullptr;
    if (expression5 != nullptr) {
        param5 = new ParameterAssign();
        setCodeInfo(param5);
        param5->setName("param5");
        param5->setValue(expression5);
    }

    ParameterAssign *param6 = nullptr;
    if (expression6 != nullptr) {
        param6 = new ParameterAssign();
        setCodeInfo(param5);
        param6->setName("param6");
        param6->setValue(expression6);
    }

    auto *ret = new FunctionCall();
    setCodeInfo(ret);
    ret->setName(function_name);
    ret->parameterAssigns.push_back(param1);
    if (expression2 != nullptr) {
        ret->parameterAssigns.push_back(param2);
    }
    if (expression3 != nullptr) {
        ret->parameterAssigns.push_back(param3);
    }
    if (expression4 != nullptr) {
        ret->parameterAssigns.push_back(param4);
    }
    if (expression5 != nullptr) {
        ret->parameterAssigns.push_back(param5);
    }
    if (expression6 != nullptr) {
        ret->parameterAssigns.push_back(param6);
    }

    return ret;
}

auto VerilogParser::parse_AnalogFilterFunctionCallArg(
    const char *function_name,
    Value *expression1,
    analog_filter_function_arg_t *expression2,
    analog_filter_function_arg_t *expression3,
    Value *expression4,
    Value *expression5,
    Value *expression6) -> FunctionCall *
{
    return parse_AnalogFilterFunctionCall(
        function_name, expression1, _makeValueFromFilter(expression2), _makeValueFromFilter(expression3), expression4,
        expression5, expression6);
}

auto VerilogParser::parse_AnalogSmallSignalFunctionCall(
    const char *function_name,
    Value *expression1,
    Value *expression2,
    Value *expression3) -> FunctionCall *
{
    ParameterAssign *param1 = nullptr;
    if (expression1 != nullptr) {
        param1 = new ParameterAssign();
        setCodeInfo(param1);
        param1->setName("param1");
        param1->setValue(expression1);
    }

    ParameterAssign *param2 = nullptr;
    if (expression2 != nullptr) {
        param2 = new ParameterAssign();
        setCodeInfo(param2);
        param2->setName("param2");
        param2->setValue(expression2);
    }

    ParameterAssign *param3 = nullptr;
    if (expression3 != nullptr) {
        param3 = new ParameterAssign();
        setCodeInfo(param3);
        param3->setName("param3");
        param3->setValue(expression3);
    }

    auto *ret = new FunctionCall();
    setCodeInfo(ret);
    ret->setName(function_name);
    if (expression1 != nullptr) {
        ret->parameterAssigns.push_back(param1);
    }
    if (expression2 != nullptr) {
        ret->parameterAssigns.push_back(param2);
    }
    if (expression3 != nullptr) {
        ret->parameterAssigns.push_back(param3);
    }

    return ret;
}

auto VerilogParser::parse_analogEventFunction(
    const char *function_name,
    Value *expression1,
    Value *expression2,
    Value *expression3,
    Value *expression4,
    Value *expression5) -> FunctionCall *
{
    auto *param1 = new ParameterAssign();
    setCodeInfo(param1);
    param1->setName("param1");
    param1->setValue(expression1);

    ParameterAssign *param2 = nullptr;
    if (expression2 != nullptr) {
        param2 = new ParameterAssign();
        setCodeInfo(param2);
        param2->setName("param2");
        param2->setValue(expression2);
    }

    ParameterAssign *param3 = nullptr;
    if (expression3 != nullptr) {
        param3 = new ParameterAssign();
        setCodeInfo(param3);
        param3->setName("param3");
        param3->setValue(expression3);
    }

    ParameterAssign *param4 = nullptr;
    if (expression4 != nullptr) {
        param4 = new ParameterAssign();
        setCodeInfo(param4);
        param4->setName("param4");
        param4->setValue(expression4);
    }

    ParameterAssign *param5 = nullptr;
    if (expression5 != nullptr) {
        param5 = new ParameterAssign();
        setCodeInfo(param5);
        param5->setName("param5");
        param5->setValue(expression5);
    }

    auto *ret = new FunctionCall();
    setCodeInfo(ret);
    ret->setName(function_name);
    ret->parameterAssigns.push_back(param1);
    if (expression2 != nullptr) {
        ret->parameterAssigns.push_back(param2);
    }
    if (expression3 != nullptr) {
        ret->parameterAssigns.push_back(param3);
    }
    if (expression4 != nullptr) {
        ret->parameterAssigns.push_back(param4);
    }
    if (expression5 != nullptr) {
        ret->parameterAssigns.push_back(param5);
    }

    return ret;
}

auto VerilogParser::parse_EventExpressionDriverUpdate(hif::Value *expression) -> event_expression_t *
{
    auto *ret       = new event_expression_t();
    ret->expression = _factory.functionCall(
        "driver_update", nullptr, _factory.noTemplateArguments(), _factory.parameterArgument("param1", expression));
    setCodeInfo(ret->expression);
    _factory.codeInfo(
        ret->expression, ret->expression->getSourceFileName(), ret->expression->getSourceLineNumber(),
        ret->expression->getSourceColumnNumber());

    return ret;
}

auto VerilogParser::parse_SystemTaskEnable(
    const char *systemIdentifier,
    hif::Value *expression_opt,
    hif::BList<hif::Value> *expression_comma_list) -> hif::ProcedureCall *
{
    auto *ret = new ProcedureCall();
    setCodeInfo(ret);
    ret->setName(systemIdentifier);

    if (expression_opt != nullptr) {
        auto *param = new ParameterAssign();
        setCodeInfo(param);
        param->setName("param1");
        param->setValue(expression_opt);
        ret->parameterAssigns.push_back(param);
    }

    if (expression_comma_list == nullptr) {
        return ret;
    }

    unsigned int paramIndex = 2;
    std::stringstream ss;
    for (hif::BList<hif::Value>::iterator it(expression_comma_list->begin()); it != expression_comma_list->end();
         ++it) {
        ss.str("");
        ss << "param" << paramIndex++;

        auto *param = new ParameterAssign();
        setCodeInfo(param);
        param->setName(ss.str());
        param->setValue(hif::copy(*it));
        ret->parameterAssigns.push_back(param);
    }

    delete expression_comma_list;
    return ret;
}

auto VerilogParser::parse_TaskEnable(Value *hierarchical_identifier, BList<Value> *comma_expression_list)
    -> ProcedureCall *
{
    auto *ret = new ProcedureCall();
    setCodeInfo(ret);
    auto *id = dynamic_cast<Identifier *>(hierarchical_identifier);
    messageAssert(id != nullptr, "Unexpected hierarchical identifier for task enable.", hierarchical_identifier, _sem);
    ret->setName(id->getName());
    delete id;

    if (comma_expression_list == nullptr) {
        return ret;
    }

    for (hif::BList<hif::Value>::iterator it(comma_expression_list->begin()); it != comma_expression_list->end();) {
        Value *v    = *it;
        it          = it.remove();
        auto *param = new ParameterAssign();
        setCodeInfo(param);
        param->setValue(v);
        ret->parameterAssigns.push_back(param);
    }

    delete comma_expression_list;
    return ret;
}

auto VerilogParser::parse_NamedPortConnectionList(char *identifier, Value *expression_opt) -> PortAssign *
{
    if (expression_opt == nullptr) {
        return nullptr;
    }

    auto *portAssign_o = new PortAssign();
    setCodeInfo(portAssign_o);

    portAssign_o->setName(identifier);
    portAssign_o->setValue(expression_opt);

    free(identifier);
    return portAssign_o;
}

auto VerilogParser::parse_ModuleInstance(
    std::list<net_ams_decl_identifier_assignment_t *> *net_ams_decl_identifier_assignment_list)
    -> module_instance_and_net_ams_decl_identifier_assignment_t *
{
    auto *wrapper                                    = new module_instance_and_net_ams_decl_identifier_assignment_t();
    wrapper->net_ams_decl_identifier_assignment_list = net_ams_decl_identifier_assignment_list;

    return wrapper;
}

auto VerilogParser::parse_ModuleInstance(
    Identifier *name_of_module_instance,
    list_of_port_connections_t *list_of_port_connections_opt)
    -> module_instance_and_net_ams_decl_identifier_assignment_t *
{
    auto *ret     = new Instance();
    auto *viewref = new ViewReference();

    setCodeInfo(ret);
    setCodeInfo(viewref);

    viewref->setName("behav");
    ret->setName(name_of_module_instance->getName());
    ret->setReferencedType(viewref);

    // named assignment
    if (list_of_port_connections_opt != nullptr &&
        list_of_port_connections_opt->named_port_connection_list != nullptr) {
        BList<PortAssign> *assign_list = list_of_port_connections_opt->named_port_connection_list;

        ret->portAssigns.merge(*assign_list);
        delete assign_list;
    } else if (
        list_of_port_connections_opt != nullptr &&
        list_of_port_connections_opt->ordered_port_connection_list != nullptr) {
        BList<Value> *val_list = list_of_port_connections_opt->ordered_port_connection_list;

        for (BList<Value>::iterator i = val_list->begin(); i != val_list->end();) {
            Value *value_o = *i;
            i              = i.remove();

            auto *portAssign_o = new PortAssign();

            auto *identifier = dynamic_cast<Identifier *>(value_o);
            if (identifier != nullptr && hif::NameTable::isDefaultValue(identifier->getName())) {
                // This is a open bind.
                // Ref design: verilog/trusthub/aes_t100_tj
                setCodeInfo(portAssign_o);
                delete value_o;
            } else {
                setCodeInfo(portAssign_o);
                portAssign_o->setValue(value_o);
                //portAssign_o->setName( NameTable::getInstance()->none() );
            }

            ret->portAssigns.push_back(portAssign_o);
        }
        delete val_list;
    }

    auto *wrapper                    = new module_instance_and_net_ams_decl_identifier_assignment_t();
    wrapper->name_of_module_instance = ret;

    delete list_of_port_connections_opt;
    delete name_of_module_instance;

    return wrapper;
}

auto VerilogParser::parse_ModuleInstantiation(
    char *identifier,
    std::list<module_instance_and_net_ams_decl_identifier_assignment_t *> *module_instance_list)
    -> std::list<module_instance_and_net_ams_decl_identifier_assignment_t *> *
{
    for (auto &i : *module_instance_list) {
        if (i->name_of_module_instance != nullptr) {
            auto *viewref_o = dynamic_cast<ViewReference *>(i->name_of_module_instance->getReferencedType());
            messageDebugAssert(
                viewref_o != nullptr, "Unexpected ref type", i->name_of_module_instance->getReferencedType(), _sem);
            viewref_o->setDesignUnit(identifier);
        } else if (i->net_ams_decl_identifier_assignment_list != nullptr) {
            TypeReference tr;
            setCodeInfo(&tr);
            tr.setName(identifier);

            i->ams_created_variables = new BList<Signal>();

            for (auto it(i->net_ams_decl_identifier_assignment_list->begin());
                 it != i->net_ams_decl_identifier_assignment_list->end(); ++it) {
                messageAssert((*it)->dimension_list == nullptr, "Unexpected dimension_list", nullptr, nullptr);

                auto *id = dynamic_cast<Identifier *>((*it)->identifier);
                messageAssert(id != nullptr, "Expected Identifier", (*it)->identifier, nullptr);

                auto *v = new Signal();
                setCodeInfo(v);
                v->setName(id->getName());
                v->setValue((*it)->init_expression);
                v->setType(hif::copy(&tr));

                delete (*it)->identifier;

                i->ams_created_variables->push_back(v);

                delete *it;
            }

            delete i->net_ams_decl_identifier_assignment_list;
        } else {
            messageDebugAssert(false, "Unexpected case", nullptr, _sem);
        }
    }

    free(identifier);
    return module_instance_list;
}

auto VerilogParser::parse_ModuleInstantiation(
    char *identifier,
    BList<ValueTPAssign> *parameter_value_assignment,
    std::list<module_instance_and_net_ams_decl_identifier_assignment_t *> *module_instance_list)
    -> std::list<module_instance_and_net_ams_decl_identifier_assignment_t *> *
{
    std::list<module_instance_and_net_ams_decl_identifier_assignment_t *> *inst_l =
        this->parse_ModuleInstantiation(identifier, module_instance_list);

    for (auto &i : *inst_l) {
        if (i->name_of_module_instance != nullptr) {
            auto *viewref_o = dynamic_cast<ViewReference *>(i->name_of_module_instance->getReferencedType());
            messageAssert(
                viewref_o != nullptr, "Unexpected ref type", i->name_of_module_instance->getReferencedType(), _sem);

            for (BList<ValueTPAssign>::iterator j = parameter_value_assignment->begin();
                 j != parameter_value_assignment->end(); ++j) {
                viewref_o->templateParameterAssigns.push_back(hif::copy(*j));
            }
        } else {
            messageDebugAssert(false, "Unexpected case", nullptr, _sem);
        }
    }

    delete parameter_value_assignment;
    return module_instance_list;
}

auto VerilogParser::parse_ModuleInstantiation(
    char *identifier,
    Range *range_opt,
    std::list<module_instance_and_net_ams_decl_identifier_assignment_t *> *module_instance_list)
    -> std::list<module_instance_and_net_ams_decl_identifier_assignment_t *> *
{
    std::list<module_instance_and_net_ams_decl_identifier_assignment_t *> *inst_l =
        this->parse_ModuleInstantiation(identifier, module_instance_list);

    if (range_opt == nullptr) {
        return module_instance_list;
    }

    for (auto &i : *inst_l) {
        hif::BList<hif::Signal> *ams_created_variables = i->ams_created_variables;
        messageAssert(ams_created_variables != nullptr, "Unexpected case", nullptr, _sem);

        for (BList<Signal>::iterator j = ams_created_variables->begin(); j != ams_created_variables->end(); ++j) {
            auto *type = new Array();
            type->setSpan(hif::copy(range_opt));
            type->setType((*j)->setType(nullptr));
            (*j)->setType(type);
        }
    }

    delete range_opt;
    return module_instance_list;
}

auto VerilogParser::parse_ModuleOrGenerateItem(std::list<module_instance_and_net_ams_decl_identifier_assignment_t *>
                                                   *module_instantiation) -> module_or_generate_item_t *
{
    auto *ret = new module_or_generate_item_t();

    for (auto it(module_instantiation->begin()); it != module_instantiation->end();) {
        module_instance_and_net_ams_decl_identifier_assignment_t *item = *it;

        // In parent rule (parse_moduleDeclaration) we are expecting either:
        // - a single instance
        // - a list of declarations
        if (item->name_of_module_instance != nullptr) {
            if (ret->module_instantiation == nullptr) {
                ret->module_instantiation = new BList<Instance>();
            }
            ret->module_instantiation->push_back(item->name_of_module_instance);
        } else {
            if (ret->module_or_generate_item_declaration == nullptr) {
                ret->module_or_generate_item_declaration                  = new module_or_generate_item_declaration_t();
                ret->module_or_generate_item_declaration->reg_declaration = new hif::BList<hif::Declaration>();
            }
            auto *declList = reinterpret_cast<BList<Declaration> *>(item->ams_created_variables);
            if (declList != nullptr) {
                ret->module_or_generate_item_declaration->reg_declaration->merge(*declList);
                delete declList;
            }
        }

        delete item;
        it = module_instantiation->erase(it);
    }

    delete module_instantiation;

    return ret;
}

auto VerilogParser::parse_GateTerminalInstance(Identifier *name, Value *output, BList<Value> *inputs)
    -> gate_terminal_instance_t *
{
    auto *ret    = new gate_terminal_instance_t();
    ret->name    = name;
    ret->outputs = new BList<Value>();
    ret->outputs->push_back(output);
    ret->inputs = inputs;

    return ret;
}

auto VerilogParser::parse_GateTerminalInstanceMultiOutput(Identifier *name, BList<Value> *outputs, Value *input)
    -> gate_terminal_instance_t *
{
    auto *ret    = new gate_terminal_instance_t();
    ret->name    = name;
    ret->outputs = outputs;
    ret->inputs  = new BList<Value>();
    ret->inputs->push_back(input);

    return ret;
}

auto VerilogParser::parse_GateInstantiation(
    gate_primitive_kind_t kind,
    std::list<gate_terminal_instance_t *> *instances) -> BList<Assign> *
{
    // Basic Verilog gate primitives (and/nand/or/nor/xor/xnor/buf/not) have
    // no HIF library counterpart to instantiate against (unlike ordinary
    // module instantiation, which binds an Instance to a ViewReference
    // resolved later against a user-declared DesignUnit). We therefore lower
    // each gate instance directly into its equivalent combinational
    // behavior, exactly as if it had been written as a continuous
    // "assign output = <expr>(inputs);" statement. This reuses the
    // module_or_generate_item_t::continuous_assign merge path unchanged
    // (see parse_ModuleDeclaration), so gate primitives simply become part
    // of the module's concurrent actions.
    Operator reduceOp;
    bool isReducing;
    bool negateResult;
    switch (kind) {
    case GATE_PRIMITIVE_AND:
        reduceOp     = op_band;
        isReducing   = true;
        negateResult = false;
        break;
    case GATE_PRIMITIVE_NAND:
        reduceOp     = op_band;
        isReducing   = true;
        negateResult = true;
        break;
    case GATE_PRIMITIVE_OR:
        reduceOp     = op_bor;
        isReducing   = true;
        negateResult = false;
        break;
    case GATE_PRIMITIVE_NOR:
        reduceOp     = op_bor;
        isReducing   = true;
        negateResult = true;
        break;
    case GATE_PRIMITIVE_XOR:
        reduceOp     = op_bxor;
        isReducing   = true;
        negateResult = false;
        break;
    case GATE_PRIMITIVE_XNOR:
        reduceOp     = op_bxor;
        isReducing   = true;
        negateResult = true;
        break;
    case GATE_PRIMITIVE_BUF:
        reduceOp     = op_none;
        isReducing   = false;
        negateResult = false;
        break;
    case GATE_PRIMITIVE_NOT:
        reduceOp     = op_none;
        isReducing   = false;
        negateResult = true;
        break;
    default:
        messageDebugAssert(false, "Unexpected gate primitive kind", nullptr, _sem);
        reduceOp     = op_none;
        isReducing   = false;
        negateResult = false;
        break;
    }

    auto *ret = new BList<Assign>();

    for (auto *inst : *instances) {
        messageAssert(
            inst->inputs != nullptr && !inst->inputs->empty(), "Gate instance without input terminal(s)", nullptr,
            _sem);
        messageAssert(
            inst->outputs != nullptr && !inst->outputs->empty(), "Gate instance without output terminal(s)", nullptr,
            _sem);

        // Take ownership of the first input terminal, then fold in any
        // remaining ones (n-input gates only; n-output gates always have
        // exactly one input terminal by construction, so the loop below
        // never runs for them).
        BList<Value>::iterator iit = inst->inputs->begin();
        Value *driver              = *iit;
        iit                        = iit.remove();
        while (isReducing && iit != inst->inputs->end()) {
            Value *rhs = *iit;
            iit        = iit.remove();
            driver     = parse_ExpressionBinaryOperator(driver, reduceOp, rhs);
        }
        if (negateResult) {
            driver = parse_ExpressionUnaryOperator(op_bnot, driver);
        }

        // Drive every output terminal with the same expression (copied for
        // all but the first, since HIF subtrees cannot be shared).
        bool first = true;
        for (BList<Value>::iterator oit = inst->outputs->begin(); oit != inst->outputs->end();) {
            Value *outLValue = *oit;
            oit              = oit.remove();

            Assign *assign_o = parse_Assignment(outLValue, first ? driver : hif::copy(driver));
            ret->push_back(assign_o);

            first = false;
        }

        delete inst->outputs;
        delete inst->inputs;
        delete inst->name;
        delete inst;
    }

    delete instances;

    return ret;
}

auto VerilogParser::parse_NatureBinding(char *identifier, bool isPotential) -> Variable *
{
    auto *vr = new ViewReference();
    vr->setName("ams_nature");
    vr->setDesignUnit(identifier);
    auto *var = new Variable();
    var->setType(vr);
    if (isPotential) {
        var->setName("potential");
    } else {
        var->setName("flow");
    }

    free(identifier);
    return var;
}

void VerilogParser::parse_DisciplineDeclaration(char *identifier, hif::BList<Variable> *discipline_item_list)
{
    hif::HifFactory factory;
    factory.setSemantics(_sem);
    View *v = factory.view(
        "ams_discipline", nullptr, nullptr, hif::rtl, factory.noDeclarations(), factory.noLibraries(),
        factory.noTemplates(), false, hif::HifFactory::noViewReferences());
    DesignUnit *ret = factory.designUnit(identifier, v);

    for (BList<Variable>::iterator i = discipline_item_list->begin(); i != discipline_item_list->end(); ++i) {
        Variable *var = (*i);
        auto *vr      = dynamic_cast<ViewReference *>(var->getType());
        messageAssert(vr != nullptr, "Unexpected discipline_item_list", var, _sem);
        v->inheritances.push_back(hif::copy(vr));
    }
    auto *c = new Contents();
    v->setContents(c);
    c->declarations.merge(discipline_item_list->toOtherBList<Declaration>());

    delete discipline_item_list;
    free(identifier);
    _designUnits->push_back(ret);
}

auto VerilogParser::parse_AnalogFunctionDeclaration(
    bool isInteger,
    char *identifier,
    analog_function_item_declaration_t *analog_function_item_declaration_list,
    analog_statement_t *analog_function_statement) -> Function *
{
    hif::HifFactory factory;
    factory.setSemantics(_sem);

    Type *type = nullptr;
    if (isInteger) {
        type = factory.bitvector(new Range(31, 0), true, true, false, true);
    } else {
        type = factory.real();
    }

    SubProgram *s = factory.subprogram(type, identifier, factory.noTemplates(), factory.noParameters());

    s->setStateTable(factory.stateTable(identifier, factory.noDeclarations(), factory.noActions()));

    _buildActionListFromAnalogStatement(analog_function_statement, s->getStateTable()->states.front()->actions);

    BList<Declaration>::iterator i = analog_function_item_declaration_list->analog_block_item_declaration.begin();

    for (; i != analog_function_item_declaration_list->analog_block_item_declaration.end(); ++i) {
        auto *d = dynamic_cast<DataDeclaration *>(*i);
        messageAssert(d != nullptr, "Unexpected case.", *i, _sem);

        BList<Port>::iterator it = analog_function_item_declaration_list->input_declaration_identifiers.begin();

        bool found = false;
        for (; it != analog_function_item_declaration_list->input_declaration_identifiers.end(); ++it) {
            Port *p = *it;
            if (p->getName() != d->getName()) {
                continue;
            }
            auto *pp = new Parameter();
            pp->setName(p->getName());
            pp->setValue(d->setValue(nullptr));
            pp->setType(d->setType(nullptr));
            s->parameters.push_back(pp);
            found = true;
            break;
        }

        it = analog_function_item_declaration_list->output_declaration_identifiers.begin();

        messageAssert(
            it == analog_function_item_declaration_list->output_declaration_identifiers.end(), "Unsupported case.",
            nullptr, nullptr);

        it = analog_function_item_declaration_list->inout_declaration_identifiers.begin();
        messageAssert(
            it == analog_function_item_declaration_list->inout_declaration_identifiers.end(), "Unsupported case.",
            nullptr, nullptr);

        if (!found) {
            s->getStateTable()->declarations.push_back(hif::copy(d));
        }
    }

    delete analog_function_item_declaration_list;
    delete analog_function_statement;
    free(identifier);

    // Adding implicit return stmt and var ret decl:
    Variable *varRet = _factory.variable(hif::copy(type), s->getName());
    Return *ret      = _factory.retStmt(new Identifier(s->getName()));
    setCodeInfo(varRet);
    setCodeInfo(ret);
    _factory.codeInfo(varRet, varRet->getSourceFileName(), varRet->getSourceLineNumber());
    _factory.codeInfo(ret, ret->getSourceFileName(), ret->getSourceLineNumber());
    s->getStateTable()->declarations.push_back(varRet);
    s->getStateTable()->states.front()->actions.push_back(ret);

    return dynamic_cast<Function *>(s);
}

auto VerilogParser::parse_GenvarDeclaration(hif::BList<Identifier> *list_of_identifiers) -> hif::BList<Declaration> *
{
    auto *ret = new hif::BList<Declaration>();
    if (list_of_identifiers != nullptr) {
        for (hif::BList<Identifier>::iterator i = list_of_identifiers->begin(); i != list_of_identifiers->end(); ++i) {
            Identifier *id = *i;
            auto *v        = new Variable();
            setCodeInfo(v);
            v->addProperty(PROPERTY_GENVAR);
            v->setName(id->getName());
            Type *t = _factory.bitvector(
                _factory.range(new IntValue(31), hif::dir_downto, new IntValue(0)), true, true, false, true);
            _factory.codeInfo(t, v->getSourceFileName(), v->getSourceLineNumber(), v->getSourceColumnNumber());
            v->setType(t);
            ret->push_back(v);
        }
    }

    return ret;
}

auto VerilogParser::parse_SeqBlock(
    char *identifier,
    std::list<block_item_declaration_t *> *declarations,
    std::list<statement_t *> *statements) -> statement_t *
{
    auto *stm = new statement_t();

    auto *declarationsList = new BList<Declaration>();

    if (declarations != nullptr) {
        auto it = declarations->begin();
        for (; it != declarations->end(); ++it) {
            block_item_declaration_t *block_decl = *it;

            if (block_decl->local_parameter_declaration != nullptr) {
                BList<Declaration> *decl_list = blist_scast<Declaration>(block_decl->local_parameter_declaration);

                declarationsList->merge(*decl_list);
                delete block_decl->local_parameter_declaration;
            } else if (block_decl->integer_variable_declaration != nullptr) {
                BList<Declaration> *decl_list = blist_scast<Declaration>(block_decl->integer_variable_declaration);

                declarationsList->merge(*decl_list);
                delete block_decl->integer_variable_declaration;
            } else if (block_decl->reg_variable_declaration != nullptr) {
                BList<Declaration> *decl_list = blist_scast<Declaration>(block_decl->reg_variable_declaration);

                declarationsList->merge(*decl_list);
                delete block_decl->reg_variable_declaration;
            } else if (block_decl->variable_declaration != nullptr) {
                BList<Declaration> *decl_list = blist_scast<Declaration>(block_decl->variable_declaration);

                declarationsList->merge(*decl_list);
                delete block_decl->variable_declaration;
            } else {
                messageDebugAssert(false, "Unexpected declarations", nullptr, _sem);
            }
        }

        delete declarations;
    }

    stm->seq_block_actions      = statements;
    stm->seq_block_declarations = declarationsList;

    if (identifier != nullptr) {
        stm->blockName = std::string(identifier);
        free(identifier);
    }

    return stm;
}

auto VerilogParser::parse_AnalogSeqBlock(
    char *identifier,
    BList<Declaration> *declarations,
    std::list<analog_statement_t *> *statements) -> analog_statement_t *
{
    auto *stm = new analog_statement_t();

    stm->analog_seq_block_actions      = statements;
    stm->analog_seq_block_declarations = declarations;

    if (identifier != nullptr) {
        stm->blockName = std::string(identifier);
        free(identifier);
    }

    return stm;
}

auto VerilogParser::parse_SpecifyBlock(std::list<specify_item_t *> *items) -> std::list<specify_item_t *> *
{
    for (auto it = items->begin(); it != items->end();) {
        specify_item_t *specifyItem = *it;

        if (specifyItem->system_timing_check != nullptr) {
            messageWarning(
                "Skipping unsupported system-timing-check statement", specifyItem->system_timing_check, nullptr);

            delete specifyItem->system_timing_check;
            delete specifyItem;

            it = items->erase(it);

            continue;
        }
        static_assert(true);

        ++it;
    }

    return items;
}

auto VerilogParser::parse_TimingCheckEvent(
    timing_check_event_control_t *timing_check_event_control_opt,
    specify_terminal_descriptor_t *specify_terminal_descriptor) -> Value *
{
    Value *ret = specify_terminal_descriptor->identifier;

    if (specify_terminal_descriptor->range_expression != nullptr) {
        delete specify_terminal_descriptor->range_expression;
        messageError("Unsupported range_expression", nullptr, nullptr);
    }

    if (timing_check_event_control_opt != nullptr) {
        if (timing_check_event_control_opt->pos_edge) {
            ret->addProperty(PROPERTY_SENSITIVE_POS);
        } else if (timing_check_event_control_opt->neg_edge) {
            ret->addProperty(PROPERTY_SENSITIVE_NEG);
        } else {
            messageError("Unexpected case", nullptr, nullptr);
        }
    }

    delete specify_terminal_descriptor;
    delete timing_check_event_control_opt;

    return ret;
}

auto VerilogParser::parse_TimingCheck(
    const char *name,
    Value *dataEvent,
    Value *referenceEvent,
    Value *timingCheckLimit,
    Identifier *notifier) -> ProcedureCall *
{
    auto *pCall = new ProcedureCall();
    setCodeInfo(pCall);

    pCall->setName(name);

    auto *pAssign = new ParameterAssign();
    pAssign->setName("param1");
    pAssign->setValue(dataEvent);
    pCall->parameterAssigns.push_back(pAssign);

    pAssign = new ParameterAssign();
    pAssign->setName("param2");
    pAssign->setValue(referenceEvent);
    pCall->parameterAssigns.push_back(pAssign);

    pAssign = new ParameterAssign();
    pAssign->setName("param3");
    pAssign->setValue(timingCheckLimit);
    pCall->parameterAssigns.push_back(pAssign);

    pAssign = new ParameterAssign();
    pAssign->setName("param4");
    pAssign->setValue(notifier);
    pCall->parameterAssigns.push_back(pAssign);

    return pCall;
}

auto VerilogParser::parse_SpecifyTerminalDescriptor(char *identifier, Value *range_expression)
    -> specify_terminal_descriptor_t *
{
    auto *ret = new specify_terminal_descriptor_t();

    ret->identifier       = new Identifier(identifier);
    ret->range_expression = range_expression;

    free(identifier);
    return ret;
}

void VerilogParser::_fillBaseContentsFromModuleOrGenerateItem(
    BaseContents *contents_o,
    module_or_generate_item_t *mod_item,
    DesignUnit *designUnit)
{
    messageAssert(contents_o != nullptr, "Expected base contents", nullptr, _sem);
    messageAssert(mod_item != nullptr, "Expected mod item", nullptr, _sem);

    if (mod_item->initial_construct != nullptr) {
        auto *stateTable_o = new StateTable();
        setCodeInfo(stateTable_o);
        stateTable_o->setName(NameTable::getInstance()->getFreshName("initial_process"));
        stateTable_o->setFlavour(pf_initial);

        Contents *contentsOfInitialBlock = mod_item->initial_construct;

        auto *state_o = new State();
        setCodeInfo(state_o);
        state_o->setName(stateTable_o->getName());
        if (contentsOfInitialBlock->getGlobalAction() == nullptr) {
            contentsOfInitialBlock->setGlobalAction(new GlobalAction());
        }
        state_o->actions.merge(contentsOfInitialBlock->getGlobalAction()->actions);

        stateTable_o->states.push_back(state_o);

        contents_o->stateTables.push_back(stateTable_o);
        contents_o->declarations.merge(contentsOfInitialBlock->declarations);

        delete contentsOfInitialBlock;
    } else if (mod_item->local_parameter_declaration != nullptr) {
        BList<Declaration> *decl_list = blist_scast<Declaration>(mod_item->local_parameter_declaration);
        contents_o->declarations.merge(*decl_list);
        delete mod_item->local_parameter_declaration;
    } else if (mod_item->module_or_generate_item_declaration != nullptr) {
        // all other declarations
        _manageModuleItemDeclarations(contents_o, designUnit, mod_item->module_or_generate_item_declaration);
    } else if (mod_item->always_construct != nullptr) {
        contents_o->stateTables.push_back(mod_item->always_construct);
    } else if (mod_item->analog_construct != nullptr) {
        contents_o->stateTables.push_back(mod_item->analog_construct);
    } else if (mod_item->continuous_assign != nullptr) {
        BList<Action> *action_list = blist_scast<Action>(mod_item->continuous_assign);
        if (contents_o->getGlobalAction() == nullptr) {
            contents_o->setGlobalAction(new GlobalAction());
        }
        contents_o->getGlobalAction()->actions.merge(*action_list);
        delete mod_item->continuous_assign;
    } else if (mod_item->module_instantiation != nullptr) {
        contents_o->instances.merge(*mod_item->module_instantiation);
        delete mod_item->module_instantiation;
    } else {
        messageDebugAssert(false, "Unexpected case", nullptr, _sem);
    }

    delete mod_item;
}

auto VerilogParser::parse_ParameterValueAssignment(list_of_parameter_assignment_t *list_of_parameter_assignment)
    -> BList<ValueTPAssign> *
{
    auto *ret                = new BList<ValueTPAssign>();
    BList<Value> *value_list = list_of_parameter_assignment->ordered_parameter_assignment_list;

    if (value_list != nullptr) {
        for (BList<Value>::iterator i = value_list->begin(); i != value_list->end();) {
            Value *value_o = *i;
            i              = i.remove();

            auto *tp = new ValueTPAssign();
            setCodeInfo(tp);

            tp->setValue(value_o);

            ret->push_back(tp);
        }

        delete value_list;
    } else {
        ret->merge(*list_of_parameter_assignment->named_parameter_assignment_list);
        delete list_of_parameter_assignment->named_parameter_assignment_list;
    }

    delete list_of_parameter_assignment;
    return ret;
}

auto VerilogParser::parse_NamedParameterAssignment(char *identifier, Value *mintypmax_expression_opt) -> ValueTPAssign *
{
    if (mintypmax_expression_opt == nullptr) {
        free(identifier);
        return nullptr;
    }

    auto *ret = new ValueTPAssign();
    setCodeInfo(ret);

    ret->setName(identifier);
    ret->setValue(mintypmax_expression_opt);

    free(identifier);
    return ret;
}

auto VerilogParser::parse_UnsignedNumber(number_t &DEC_NUMBER) -> Value *
{
    if (DEC_NUMBER.sign) {
        std::ostringstream msg;
        msg << "Signed option is not supported. Number (" << string(DEC_NUMBER.value) << ") at line " << yylineno
            << ", column " << yycolumno;

        messageError(msg.str(), nullptr, nullptr);
    }

    auto *ret = new IntValue();
    setCodeInfo(ret);

    int ris = atoi(DEC_NUMBER.value);
    ret->setValue(ris);

    free(DEC_NUMBER.value);
    return ret;
}

auto VerilogParser::parse_BasedNumber(number_t &BASED_NUMBER) -> Value *
{
    if (BASED_NUMBER.sign) {
        std::ostringstream msg;
        msg << "Signed option is not supported. Number (" << BASED_NUMBER.type << string(BASED_NUMBER.value)
            << ") at line " << yylineno << ", column " << yycolumno;

        messageError(msg.str(), nullptr, nullptr);
    }

    int ris = 0;
    string str;
    int len              = static_cast<int>(strlen(BASED_NUMBER.value));
    IntValue *intValue_o = nullptr;
    std::size_t pos      = 0;

    Value *ret = nullptr;

    switch (BASED_NUMBER.type) {
    // DECIMAL NUMBER
    case 'd':
        ris        = atoi(BASED_NUMBER.value);
        intValue_o = new IntValue();
        intValue_o->setValue(ris);
        ret = intValue_o;
        break;

        // BINARY NUMBER
    case 'b':
        if (len > 1) {
            ret = new BitvectorValue(BASED_NUMBER.value);
        } else {
            auto *bitValue_o = new BitValue();

            switch (toupper(*BASED_NUMBER.value)) {
            case '1':
                bitValue_o->setValue(bit_one);
                break;
            case '0':
                bitValue_o->setValue(bit_zero);
                break;
            case 'X':
                bitValue_o->setValue(bit_x);
                break;
            case 'Z':
                bitValue_o->setValue(bit_z);
                break;
            default:;
            }

            ret = bitValue_o;
        }
        break;

        // OCTAL NUMBER
    case 'o':
        str.assign(BASED_NUMBER.value);

        while ((pos = str.find('x')) != string::npos || (pos = str.find('z')) != string::npos) {
            yywarning("x or z element not supported in octal form. "
                      "Replacing with zeros.");
            str[pos] = '0';
        }
        ris        = atoi(("0" + str).c_str());
        intValue_o = new IntValue();
        intValue_o->setValue(ris);
        ret = intValue_o;
        break;

        // HEXADECIMAL NUMBER
    case 'h':
        str.assign(BASED_NUMBER.value);

        while ((pos = str.find('x')) != string::npos || (pos = str.find('z')) != string::npos) {
            yywarning("x or z element not supported in hex form. "
                      "Replacing with zeros.");
            str[pos] = '0';
        }

        {
            int i = 0;
            std::stringstream ss;
            ss << std::hex << str;
            ss >> i;

            intValue_o = new IntValue();
            intValue_o->setValue(i);
            ret = intValue_o;
        }
        break;
    default:
        break;
    }

    free(BASED_NUMBER.value);

    setCodeInfo(ret);
    return ret;
}

auto VerilogParser::parse_DecNumber(number_t &BASED_NUMBER) -> Value *
{
    if (BASED_NUMBER.sign) {
        std::ostringstream msg;
        msg << "Signed option is not supported. Number (" << BASED_NUMBER.type << string(BASED_NUMBER.value)
            << ") at line " << yylineno << ", column " << yycolumno;

        messageError(msg.str(), nullptr, nullptr);
    }

    auto *intValue_o = new IntValue();
    setCodeInfo(intValue_o);

    int ris = atoi(BASED_NUMBER.value);
    intValue_o->setValue(ris);

    free(BASED_NUMBER.value);
    return intValue_o;
}

auto VerilogParser::parse_RealTime(real_number_t &REALTIME) -> Value *
{
    double ris = 0.0;
    double num = 0.0;
    double exp = 0.0;

    Value *ret = nullptr;

    if (!REALTIME.e) {
        ris = atof(REALTIME.value);
        ret = new RealValue(ris);
    } else {
        num = atof(REALTIME.value);
        exp = atof(REALTIME.exp);
        ris = num * pow(10, exp);
        ret = new RealValue(ris);
    }

    if (REALTIME.exp != nullptr) {
        free(REALTIME.exp);
    }

    if (REALTIME.value != nullptr) {
        free(REALTIME.value);
    }

    setCodeInfo(ret);
    return ret;
}

auto VerilogParser::parse_DecBasedNumber(number_t &DEC_NUMBER, number_t &BASED_NUMBER) -> Value *
{
    Value *ret = nullptr;
    string tmpResult;

    unsigned int numberOfBits = atoi(DEC_NUMBER.value);

    if (BASED_NUMBER.sign) {
        std::ostringstream msg;
        msg << "Signed option is not supported. Number (" << string(DEC_NUMBER.value) << "'" << BASED_NUMBER.type
            << string(BASED_NUMBER.value) << ") at line " << yylineno << ", column " << yycolumno;

        messageError(msg.str(), nullptr, nullptr);
    }

    // BINARY
    if (BASED_NUMBER.type == 'd') {
        tmpResult = convertToBinary(BASED_NUMBER.value, numberOfBits);
    }
    // DECIMAL
    else if (BASED_NUMBER.type == 'b') {
        tmpResult = string(BASED_NUMBER.value);
    }
    // OCTAL
    else if (BASED_NUMBER.type == 'o') {
        string number(BASED_NUMBER.value);

        string::iterator it = number.begin();
        for (; it != number.end(); ++it) {
            char s = *it;
            if (s == 'x' || s == 'X') {
                tmpResult.append("xxx");
            } else if (s == 'z' || s == 'Z') {
                tmpResult.append("zzz");
            } else {
                tmpResult.append(convertToBinary(s, 3));
            }
        }
    }
    // HEXADECIMAL
    else if (BASED_NUMBER.type == 'h') {
        string number(BASED_NUMBER.value);

        string::iterator it = number.begin();
        for (; it != number.end(); ++it) {
            char s = *it;
            string tmp;
            switch (toupper(s)) {
            case 'A':
                tmp = convertToBinary("10", 4);
                break;
            case 'B':
                tmp = convertToBinary("11", 4);
                break;
            case 'C':
                tmp = convertToBinary("12", 4);
                break;
            case 'D':
                tmp = convertToBinary("13", 4);
                break;
            case 'E':
                tmp = convertToBinary("14", 4);
                break;
            case 'F':
                tmp = convertToBinary("15", 4);
                break;
            case 'Z':
                tmp = "zzzz";
                break;
            case 'X':
                tmp = "xxxx";
                break;
            default:
                tmp = convertToBinary(s, 4);
            }

            tmpResult.append(tmp);
        }
    } else {
        messageAssert(false, "Unexpected based decimal number", nullptr, nullptr);
    }

    string result;

    // Extension / Padding
    if (!tmpResult.empty() && tmpResult.size() < numberOfBits) {
        int padLen = static_cast<int>(numberOfBits - tmpResult.size());
        char front = tmpResult.at(0);

        if (front == 'x' || front == 'X') {
            result.append(string(padLen, 'x'));
        } else if (front == 'z' || front == 'Z') {
            result.append(string(padLen, 'z'));
        } else {
            if (BASED_NUMBER.sign) {
                result.append(string(padLen, front));
            } else {
                result.append(string(padLen, '0'));
            }
        }
    } else if (!tmpResult.empty() && tmpResult.size() > numberOfBits) {
        tmpResult = tmpResult.substr(tmpResult.size() - numberOfBits);
    }

    result.append(tmpResult);

    if (result.size() > 1) {
        ret = new BitvectorValue(result);
    } else {
        auto *bitValue_o = new BitValue();
        switch (toupper(result.at(0))) {
        case '1':
            bitValue_o->setValue(bit_one);
            break;
        case '0':
            bitValue_o->setValue(bit_zero);
            break;
        case 'X':
            bitValue_o->setValue(bit_x);
            break;
        case 'Z':
            bitValue_o->setValue(bit_z);
            break;
        default:
            messageError("Unexpected bit value", nullptr, nullptr);
        }

        ret = bitValue_o;
    }

    free(BASED_NUMBER.value);
    free(DEC_NUMBER.value);

    setCodeInfo(ret);
    return ret;
}

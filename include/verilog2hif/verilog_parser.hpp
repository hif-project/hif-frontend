/// @file verilog_parser.hpp
/// @brief Header file for the VerilogParser class.
/// Copyright (c) 2024-2025, Electronic Systems Design (ESD) Group,
/// Univeristy of Verona.
/// This file is distributed under the BSD 2-Clause License.
/// See LICENSE.md for details.

#pragma once

#include <cstdlib>

#include <hif/hif.hpp>

#include "parser_struct.hpp"

class VerilogParser;
#include "parse_line.hpp"
#include "verilog_parser.hpp"

class VerilogParser
{
public:
    VerilogParser(const std::string &fileName, const Verilog2hifParseLine &cLine);
    ~VerilogParser();

    static auto buildSystemObject() -> hif::System *;
    static void setVerilogAms(bool b);
    static auto isVerilogAms() -> bool;

    /*
     * Public functions
     * --------------------------------------------------------------------- */
    auto parse(bool parseOnly) -> bool;
    auto isParseOnly() const -> bool;
    void setCodeInfo(hif::Object *o, bool recursive = false);
    void setCodeInfo(hif::Object *o, keyword_data_t &keyword);
    void setCurrentBlockCodeInfo(keyword_data_t &keyword);
    void setCurrentBlockCodeInfo(hif::Object *other);
    void setCodeInfoFromCurrentBlock(hif::Object *o);
    void SetTimescale(hif::TimeValue *unit, hif::TimeValue *precision);
    auto getFactory() -> hif::HifFactory *;

    /*
     * VERILOG-Grammar related functions
     * --------------------------------------------------------------------- */

    auto parse_Identifier(char *identifier) -> hif::Identifier *;

    /* -----------------------------------------------------------------------
     *  VERILOG SOURCE TEXT
     * -----------------------------------------------------------------------
     */
    auto parse_ModuleDeclarationStart(char *identifier) -> hif::DesignUnit *;

    void parse_ModuleDeclaration(
        hif::DesignUnit *designUnit,
        hif::BList<hif::Declaration> *paramList,
        hif::BList<hif::Port> *list_of_ports,
        bool referencePortList,
        std::list<module_item_t *> *module_item_list);

    void parse_ModuleDeclaration(
        hif::DesignUnit *du,
        hif::BList<hif::Declaration> *paramList,
        std::list<non_port_module_item_t *> *non_port_module_item_list);

    /* -----------------------------------------------------------------------
     *  PROCEDURAL BLOCKS AND ASSIGNMENTS
     * -----------------------------------------------------------------------
     */
    auto parse_AnalogVariableAssignment(hif::Value *lvalue, hif::Value *expression) -> hif::Assign *;

    auto parse_InitialConstruct(statement_t *statement) -> hif::Contents *;

    auto
    parse_BlockingAssignment(hif::Value *variable_lvalue, hif::Value *expression, hif::Value *delay_or_event_control)
        -> hif::Assign *;

    auto
    parse_NonBlockingAssignment(hif::Value *variable_lvalue, hif::Value *expression, hif::Value *delay_or_event_control)
        -> hif::Assign *;

    auto parse_AlwaysConstruct(statement_t *statement) -> hif::StateTable *;

    auto parse_AnalogConstruct(analog_statement_t *statement) -> hif::StateTable *;

    auto parse_Assignment(hif::Value *lvalue, hif::Value *expression) -> hif::Assign *;

    static auto parse_ContinuousAssign(hif::Value *delay3_opt, hif::BList<hif::Assign> *list_of_net_assignments)
        -> hif::BList<hif::Assign> *;

    /* -----------------------------------------------------------------------
     *  PORT_DECLARATIONS
     * -----------------------------------------------------------------------
     */
    auto parse_InoutDeclaration(discipline_and_modifiers_t *discipline_and_modifiers, hif::Identifier *identifier)
        -> hif::Port *;

    auto parse_InoutDeclaration(
        discipline_and_modifiers_t *discipline_and_modifiers,
        hif::BList<hif::Identifier> *list_of_identifiers) -> hif::BList<hif::Port> *;

    auto parse_InputDeclaration(discipline_and_modifiers_t *discipline_and_modifiers, hif::Identifier *identifier)
        -> hif::Port *;

    auto parse_InputDeclaration(
        discipline_and_modifiers_t *discipline_and_modifiers,
        hif::BList<hif::Identifier> *list_of_identifiers) -> hif::BList<hif::Port> *;

    auto parse_OutputDeclaration(discipline_and_modifiers_t *discipline_and_modifiers, hif::Identifier *identifier)
        -> hif::Port *;

    auto parse_OutputDeclaration(bool k_signed, hif::Range *range, char *identifier, hif::Value *initVal, bool isReg)
        -> hif::Port *;

    auto parse_OutputDeclaration(hif::Type *output_variable_type, char *identifier, hif::Value *initVal) -> hif::Port *;

    auto parse_OutputDeclaration(
        discipline_and_modifiers_t *discipline_and_modifiers,
        hif::BList<hif::Identifier> *list_of_identifiers) -> hif::BList<hif::Port> *;

    static auto parse_OutputDeclaration(
        hif::Type *output_variable_type,
        hif::BList<hif::Port> *list_of_variable_port_identifiers) -> hif::BList<hif::Port> *;

    static auto parse_OutputDeclaration(
        bool k_signed,
        hif::Range *range,
        hif::BList<hif::Port> *list_of_variable_port_identifiers,
        bool isReg) -> hif::BList<hif::Port> *;

    auto parse_VariablePortIdentifier(char *identifier, hif::Value *initVal) -> hif::Port *;

    auto parse_EventIdentifier(char *identifier, hif::BList<hif::Range> *dimension_list) -> hif::Variable *;

    auto parse_PortReference(char *identifier) -> hif::Port *;
    //hif::Value * parse_PortReference( hif::Identifier * identifier, /* RANGE_EXPRESSION */ );

    /* -----------------------------------------------------------------------
     *  TYPE DECLARATIONS
     * -----------------------------------------------------------------------
     */

    auto parse_BranchDeclaration(
        hif::BList<hif::Value> *branch_terminal_list,
        hif::BList<hif::Identifier> *list_of_identifiers) -> hif::BList<hif::Declaration> *;
    auto parse_BranchTerminal(char *identifier, hif::Value *range_expression) -> hif::Member *;

    /* -----------------------------------------------------------------------
     *  MODULE PARAMETER DECLARATIONS
     * -----------------------------------------------------------------------
     */
    auto parse_ParameterDeclaration(
        bool k_signed,
        hif::Range *range,
        hif::BList<hif::Assign> *list_of_param_assignments,
        hif::Type *parameter_type) -> hif::BList<hif::ValueTP> *;

    auto parse_LocalParameterDeclaration(
        bool K_signed_opt,
        hif::Range *range_opt,
        hif::BList<hif::Assign> *list_of_param_assignments,
        hif::Type *parameter_type) -> hif::BList<hif::Const> *;

    /* -----------------------------------------------------------------------
     *  DECLARATION ASSIGNMENTS
     * -----------------------------------------------------------------------
     */
    auto parse_ParamAssignment(
        char *identifier,
        hif::Value *expr,
        hif::Range *range_opt                          = nullptr,
        hif::BList<hif::Value *> *value_range_list_opt = nullptr) -> hif::Assign *;

    /* -----------------------------------------------------------------------
     *  DECLARATION RANGES
     * -----------------------------------------------------------------------
     */
    auto parse_Range(hif::Value *lbound, hif::Value *rbound) -> hif::Range *;

    /* -----------------------------------------------------------------------
     *  EXPRESSIONS - NUMBERS
     * -----------------------------------------------------------------------
     */
    auto parse_UnsignedNumber(number_t &DEC_NUMBER) -> hif::Value *;
    auto parse_BasedNumber(number_t &BASED_NUMBER) -> hif::Value *;
    auto parse_DecNumber(number_t &BASED_NUMBER) -> hif::Value *;
    auto parse_DecBasedNumber(number_t &DEC_NUMBER, number_t &BASED_NUMBER) -> hif::Value *;
    auto parse_RealTime(real_number_t &REALTIME) -> hif::Value *;

    /* -----------------------------------------------------------------------
     *  EXPRESSIONS
     * -----------------------------------------------------------------------
     */
    auto
    parse_ExpressionUnaryOperator(hif::Operator unary_op, hif::Value *primary, bool negate = false) -> hif::Value *;

    auto parse_ExpressionBinaryOperator(
        hif::Value *expression1,
        hif::Operator binary_op,
        hif::Value *expression2,
        bool negate = false) -> hif::Value *;

    auto parse_ExpressionTernaryOperator(hif::Value *expression1, hif::Value *expression2, hif::Value *expression3)
        -> hif::Value *;

    static auto parse_ExpressionNorOperator(hif::Value *primary) -> hif::Value *;

    auto parse_RangeExpression(hif::Value *lbound, hif::Value *rbound) -> hif::Value *;

    auto parse_RangeExpressionPO_POS(hif::Value *lbound, hif::Value *rbound) -> hif::Value *;

    auto parse_RangeExpressionPO_NEG(hif::Value *lbound, hif::Value *rbound) -> hif::Value *;

    /* -----------------------------------------------------------------------
     *  EVENT STATEMENTS
     * -----------------------------------------------------------------------
     */
    auto parse_EventTrigger(hif::Value *hierarchical_identifier, hif::BList<hif::Value> *bracket_expression_list)
        -> hif::ValueStatement *;

    /* -----------------------------------------------------------------------
     *  EXPRESSIONS - PRIMARIES
     * -----------------------------------------------------------------------
     */
    auto
    parse_PrimaryListOfMemberOrSlice(char *identifier, hif::BList<hif::Value> *range_expression_list) -> hif::Value *;

    /* -----------------------------------------------------------------------
     *  EXPRESSIONS - CONCATENATIONS
     * -----------------------------------------------------------------------
     */
    auto parse_Concatenation(hif::BList<hif::Value> *value_list) -> hif::Value *;

    auto parse_MultipleConcatenation(hif::Value *expression, hif::Value *concatenation) -> hif::Value *;

    static auto parse_ArrayInitialization(hif::BList<hif::Value> *value_list) -> hif::Value *;

    /* -----------------------------------------------------------------------
     *  GENERAL - IDENTIFIERS
     * -----------------------------------------------------------------------
     */
    auto parse_HierarchicalIdentifier(hif::Value *hierarchical_identifier, hif::Value *hierarchical_identifier_item)
        -> hif::Value *;

    auto parse_HierarchicalIdentifierItem(char *identifier) -> hif::Value *;

    /* -----------------------------------------------------------------------
     *  TYPE_DECLARATIONS
     * -----------------------------------------------------------------------
     */

    auto parse_Type(char *identifier, hif::BList<hif::Range> *non_empty_dimension_list) -> hif::Signal *;

    auto parse_Type(char *identifier, hif::Value *expression) -> hif::Signal *;

    auto parse_Type(char *identifier) -> hif::Signal *;

    auto
    parse_EventDeclaration(hif::BList<hif::Variable> *list_of_variable_identifiers) -> hif::BList<hif::Declaration> *;

    static auto
    parse_IntegerDeclaration(hif::BList<hif::Signal> *list_of_variable_identifiers) -> hif::BList<hif::Declaration> *;

    auto parse_RealDeclaration(hif::BList<hif::Signal> *list_of_real_identifiers) -> hif::BList<hif::Declaration> *;

    static auto parse_RegDeclaration(
        discipline_identifier_signed_range_t *discipline_identifier_signed_range,
        hif::BList<hif::Signal> *list_of_variable_identifiers) -> hif::BList<hif::Declaration> *;

    static auto
    parse_TimeDeclaration(hif::BList<hif::Signal> *list_of_variable_identifiers) -> hif::BList<hif::Declaration> *;

    auto parse_NetDeclaration(
        bool is_signed,
        hif::Range *range,
        std::list<net_ams_decl_identifier_assignment_t *> *identifiers_or_assign,
        hif::Type *explicitType = nullptr) -> hif::BList<hif::Declaration> *;

    /* -----------------------------------------------------------------------
     *  EXPRESSIONS - FUNCTION CALLS
     * -----------------------------------------------------------------------
     */

    auto parse_InitialOrFinalStep(const std::string &name, std::list<std::string> *string_list) -> hif::FunctionCall *;

    auto
    parse_FunctionCall(hif::Value *hierarchical_identifier, hif::BList<hif::Value> *expression_list) -> hif::Value *;

    /* -----------------------------------------------------------------------
     *  EXPRESSIONS - NATURE ATTRIBUTE REFERENCE
     * -----------------------------------------------------------------------
     */
    auto parse_NatureAttributeReference(
        hif::Value *hierarchical_identifier,
        const std::string &fieldName,
        hif::Value *nature_attribute_identifier) -> hif::Value *;

    auto parse_AmsFlowOfPort(hif::Value *hierarchical_identifier, hif::Value *expression_list) -> hif::Value *;

    /* -----------------------------------------------------------------------
     *  CASE STATEMENTS
     * -----------------------------------------------------------------------
     */
    auto
    parse_CaseStatement(hif::Value *expression, hif::BList<hif::SwitchAlt> *case_item_list, hif::CaseSemantics caseSem)
        -> hif::Switch *;

    auto parse_CaseItem(hif::BList<hif::Value> *expression_list, statement_t *statement_or_null) -> hif::SwitchAlt *;

    auto parse_AnalogCaseItem(hif::BList<hif::Value> *expression_list, analog_statement_t *analog_statement_or_null)
        -> hif::SwitchAlt *;

    /* -----------------------------------------------------------------------
     *  CONDITIONAL STATEMENTS
     * -----------------------------------------------------------------------
     */

    auto parse_ConditionalStatement(hif::If *ifStatement) -> hif::If *;

    auto parse_ConditionalStatement(
        hif::Value *expression,
        statement_t *statement_or_null,
        statement_t *else_statement_or_null) -> hif::If *;

    auto parse_AnalogConditionalStatement(
        hif::Value *expression,
        analog_statement_t *analog_statement_or_null,
        analog_statement_t *else_analog_statement_or_null) -> hif::If *;

    auto parse_ElseIfStatementOrNullList(
        hif::BList<hif::IfAlt> *elseif_statement_or_null_list,
        hif::Value *expression,
        statement_t *statement_or_null) -> hif::BList<hif::IfAlt> *;

    /* -----------------------------------------------------------------------
     *  GENERATE STATEMENTS
     * -----------------------------------------------------------------------
     */

    auto parse_IfGenerateConstruct(
        hif::Value *expression,
        generate_block_t *generate_block_or_null_if,
        generate_block_t *generate_block_or_null_else) -> hif::BList<hif::Generate> *;

    auto parse_LoopGenerateConstruct(
        hif::Assign *genvar_initialization,
        hif::Value *expression,
        hif::Assign *genvar_iteration,
        generate_block_t *generate_block) -> hif::BList<hif::Generate> *;

    /* -----------------------------------------------------------------------
     *  TIMING CONTROL STATEMENTS
     * -----------------------------------------------------------------------
     */

    static auto parse_ProceduralTimingControlStatement(
        procedural_timing_control_t *procedural_timing_control,
        statement_t *stat_or_null) -> statement_t *;

    auto parse_AnalogEventControlStatement(analog_event_control_t *event_control, analog_statement_t *stat_or_null)
        -> analog_statement_t *;

    auto parse_WaitStatement(hif::Value *expression, statement_t *statement_or_null) -> statement_t *;

    auto parseOrAnalogEventExpression(analog_event_expression_t *e1, analog_event_expression_t *e2)
        -> analog_event_expression_t *;

    /* -----------------------------------------------------------------------
     *  LOOPING STATEMENTS
     * -----------------------------------------------------------------------
     */
    template <typename T> auto parse_LoopStatementWhile(hif::Value *expression, T *statement) -> hif::While *;
    template <typename T>
    auto parse_LoopStatementFor(
        hif::Assign *init_variable_assign,
        hif::Value *expression,
        hif::Assign *step_variable_assign,
        T *statement) -> hif::For *;
    template <typename T> auto parse_LoopStatementRepeat(hif::Value *expression, T *statement) -> hif::For *;
    auto parse_LoopStatementForever(statement_t *statement) -> hif::While *;

    /* -----------------------------------------------------------------------
     *  TASK DECLARATIONS
     * -----------------------------------------------------------------------
     */
    auto parse_TaskDeclaration(
        bool isAutomatic,
        char *identifier,
        std::list<task_item_declaration_t *> *task_item_declaration_list,
        statement_t *statement_or_null) -> hif::Procedure *;

    //    hif::Procedure * parse_TaskDeclaration( hif::Identifier * identifier,
    //            hif::BList<hif::Port> * task_port_list,
    //            hif::BList<block_item_declaration_t> * block_item_declaration_list,
    //            hif::BList<hif::Action> * statements);

    auto parse_TfDeclaration(
        hif::PortDirection dir,
        bool isSigned,
        hif::Range *range_opt,
        hif::Identifier *identifier,
        hif::Type *task_port_type) -> hif::Port *;

    auto parse_TfDeclaration(
        hif::PortDirection dir,
        bool isSigned,
        hif::Range *range_opt,
        hif::BList<hif::Identifier> *list_of_identifiers,
        hif::Type *task_port_type) -> hif::BList<hif::Port> *;

    static auto parse_BlockItemDeclaration_Reg(
        bool signed_opt,
        hif::Range *range_opt,
        hif::BList<hif::Signal> *list_of_block_variable_identifiers) -> hif::BList<hif::Signal> *;

    static auto parse_BlockItemDeclaration_Integer(hif::BList<hif::Signal> *list_of_block_variable_identifiers)
        -> hif::BList<hif::Signal> *;

    auto parse_BlockVariableType(char *identifier, hif::BList<hif::Range> *dimension_list) -> hif::Signal *;

    auto parse_AnalogSeqBlock(
        const char *block_identifier,
        hif::BList<hif::Declaration> *analog_block_item_declaration_list,
        hif::StateTable *analog_statement_no_empty_list) -> hif::StateTable *;

    auto parse_DisableStatement(hif::Value *hierarchical_identifier) -> hif::Break *;

    /* -----------------------------------------------------------------------
     *  FUNCTION DECLARATIONS
     * -----------------------------------------------------------------------
     */
    auto parse_FunctionDeclaration(
        hif::Type *function_range_or_type,
        char *identifier,
        std::list<function_item_declaration_t *> *function_item_declaration_list,
        statement_t *statements) -> hif::Function *;

    auto parse_FunctionDeclaration(
        hif::Type *function_range_or_type,
        char *identifier,
        hif::BList<hif::Port> *function_port_list,
        std::list<block_item_declaration_t *> *block_item_declaration_list,
        statement_t *statements) -> hif::Function *;

    /* -----------------------------------------------------------------------
     *  FUNCTION/PROCEDURE CALLS
     * -----------------------------------------------------------------------
     */
    auto parse_BranchProbeFunctionCall(
        hif::Value *nature_attribute_identifier,
        hif::Value *hierarchical_identifier1,
        hif::Value *hierarchical_identifier2) -> hif::FunctionCall *;

    auto parse_AnalogDifferentialFunctionCall(
        const char *function_name,
        hif::Value *expression1,
        hif::Value *expression2 = nullptr,
        hif::Value *expression3 = nullptr,
        hif::Value *expression4 = nullptr) -> hif::FunctionCall *;

    auto
    parse_ContributionStatement(hif::Value *branch_probe_function_call, hif::Value *expression) -> hif::ProcedureCall *;

    auto parse_IndirectContributionStatement(
        hif::Value *branch_probe_function_call,
        hif::Value *indirect_expression,
        hif::Value *expression) -> hif::ProcedureCall *;

    auto parse_AnalogBuiltInFunctionCall(
        hif::Identifier *analog_built_in_function_name,
        hif::Value *analog_expression1,
        hif::Value *analog_expression2) -> hif::Value *;

    auto parse_analysisFunctionCall(std::list<std::string> *string_list) -> hif::FunctionCall *;

    auto parse_AnalogFilterFunctionCall(
        const char *function_name,
        hif::Value *expression1,
        hif::Value *expression2,
        hif::Value *expression3,
        hif::Value *expression4,
        hif::Value *expression5,
        hif::Value *expression6) -> hif::FunctionCall *;

    auto parse_AnalogFilterFunctionCallArg(
        const char *function_name,
        hif::Value *expression1,
        analog_filter_function_arg_t *expression2,
        analog_filter_function_arg_t *expression3,
        hif::Value *expression4,
        hif::Value *expression5,
        hif::Value *expression6) -> hif::FunctionCall *;

    auto parse_AnalogSmallSignalFunctionCall(
        const char *function_name,
        hif::Value *expression1,
        hif::Value *expression2,
        hif::Value *expression3) -> hif::FunctionCall *;

    auto parse_analogEventFunction(
        const char *function_name,
        hif::Value *expression1,
        hif::Value *expression2 = nullptr,
        hif::Value *expression3 = nullptr,
        hif::Value *expression4 = nullptr,
        hif::Value *expression5 = nullptr) -> hif::FunctionCall *;

    auto parse_EventExpressionDriverUpdate(hif::Value *expression) -> event_expression_t *;

    auto parse_SystemTaskEnable(
        const char *systemIdentifier,
        hif::Value *expression_opt,
        hif::BList<hif::Value> *expression_comma_list) -> hif::ProcedureCall *;

    auto parse_TaskEnable(hif::Value *hierarchical_identifier, hif::BList<hif::Value> *comma_expression_list)
        -> hif::ProcedureCall *;

    /* -----------------------------------------------------------------------
     *  MODULE INSTATIATION
     * -----------------------------------------------------------------------
     */
    auto parse_NamedPortConnectionList(char *identifier, hif::Value *expression_opt) -> hif::PortAssign *;

    static auto
    parse_ModuleInstance(std::list<net_ams_decl_identifier_assignment_t *> *net_ams_decl_identifier_assignment_list)
        -> module_instance_and_net_ams_decl_identifier_assignment_t *;

    auto parse_ModuleInstance(
        hif::Identifier *name_of_module_instance,
        list_of_port_connections_t *list_of_port_connections_opt)
        -> module_instance_and_net_ams_decl_identifier_assignment_t *;

    auto parse_ModuleInstantiation(
        char *identifier,
        std::list<module_instance_and_net_ams_decl_identifier_assignment_t *> *module_instance_list)
        -> std::list<module_instance_and_net_ams_decl_identifier_assignment_t *> *;

    auto parse_ModuleInstantiation(
        char *identifier,
        hif::BList<hif::ValueTPAssign> *parameter_value_assignment,
        std::list<module_instance_and_net_ams_decl_identifier_assignment_t *> *module_instance_list)
        -> std::list<module_instance_and_net_ams_decl_identifier_assignment_t *> *;

    auto parse_ModuleInstantiation(
        char *identifier,
        hif::Range *range_opt,
        std::list<module_instance_and_net_ams_decl_identifier_assignment_t *> *module_instance_list)
        -> std::list<module_instance_and_net_ams_decl_identifier_assignment_t *> *;

    auto parse_ParameterValueAssignment(list_of_parameter_assignment_t *list_of_parameter_assignment)
        -> hif::BList<hif::ValueTPAssign> *;

    auto parse_NamedParameterAssignment(char *identifier, hif::Value *mintypmax_expression_opt) -> hif::ValueTPAssign *;

    static auto parse_ModuleOrGenerateItem(std::list<module_instance_and_net_ams_decl_identifier_assignment_t *>
                                               *module_instantiation) -> module_or_generate_item_t *;

    /* -----------------------------------------------------------------------
     *  GATE PRIMITIVE INSTATIATION
     * -----------------------------------------------------------------------
     */
    static auto parse_GateTerminalInstance(hif::Identifier *name, hif::Value *output, hif::BList<hif::Value> *inputs)
        -> gate_terminal_instance_t *;

    static auto parse_GateTerminalInstanceMultiOutput(
        hif::Identifier *name,
        hif::BList<hif::Value> *outputs,
        hif::Value *input) -> gate_terminal_instance_t *;

    auto parse_GateInstantiation(gate_primitive_kind_t kind, std::list<gate_terminal_instance_t *> *instances)
        -> hif::BList<hif::Assign> *;

    static auto parse_NatureBinding(char *identifier, bool isPotential) -> hif::Variable *;
    void parse_DisciplineDeclaration(char *identifier, hif::BList<hif::Variable> *discipline_item_list);

    auto parse_AnalogFunctionDeclaration(
        bool isInteger,
        char *identifier,
        analog_function_item_declaration_t *analog_function_item_declaration_list,
        analog_statement_t *analog_function_statement) -> hif::Function *;

    auto parse_GenvarDeclaration(hif::BList<hif::Identifier> *list_of_identifiers) -> hif::BList<hif::Declaration> *;

    /* -----------------------------------------------------------------------
     *  PARALLEL AND SEQUENTIAL BLOCKS
     * -----------------------------------------------------------------------
     */
    auto parse_SeqBlock(
        char *identifier,
        std::list<block_item_declaration_t *> *declarations,
        std::list<statement_t *> *statements) -> statement_t *;

    static auto parse_AnalogSeqBlock(
        char *identifier,
        hif::BList<hif::Declaration> *declarations,
        std::list<analog_statement_t *> *statements) -> analog_statement_t *;

    /* -----------------------------------------------------------------------
     *  SPECIFY SECTION
     *
     *  SPECIFY BLOCK DECLARATION
     * -----------------------------------------------------------------------
     */

    static auto parse_SpecifyBlock(std::list<specify_item_t *> *items) -> std::list<specify_item_t *> *;

    /* -----------------------------------------------------------------------
     *  SYSTEM TIMING CHECK COMMANDS
     * -----------------------------------------------------------------------
     */

    static auto parse_TimingCheckEvent(
        timing_check_event_control_t *timing_check_event_control_opt,
        specify_terminal_descriptor_t *specify_terminal_descriptor) -> hif::Value *;

    auto parse_TimingCheck(
        const char *name,
        hif::Value *dataEvent,
        hif::Value *referenceEvent,
        hif::Value *timingCheckLimit,
        hif::Identifier *notifier) -> hif::ProcedureCall *;

    static auto
    parse_SpecifyTerminalDescriptor(char *identifier, hif::Value *range_expression) -> specify_terminal_descriptor_t *;

private:
    VerilogParser(const VerilogParser &);
    auto operator=(const VerilogParser &) -> VerilogParser &;

    std::string _fileName;
    unsigned int _tmpCustomLineNumber;
    unsigned int _tmpCustomColumnNumber;
    bool _parseOnly;
    hif::TimeValue *_unit;
    hif::TimeValue *_precision;
    hif::semantics::ILanguageSemantics *_sem;
    hif::HifFactory _factory;

    static hif::BList<hif::DesignUnit> *_designUnits;
    static bool _isVerilogAms;

    const Verilog2hifParseLine &_cLine;

    void _fillBaseContentsFromModuleOrGenerateItem(
        hif::BaseContents *contents_o,
        module_or_generate_item_t *mod_item,
        hif::DesignUnit *designUnit = nullptr);

    void _buildActionList(
        statement_t *statement,
        hif::BList<hif::Action> &actionList,
        hif::BList<hif::Declaration> *declList = nullptr);

    void _buildActionList(analog_statement_t *statement, hif::BList<hif::Action> &actionList);

    void _buildActionListFromStatement(
        statement_t *statement,
        hif::BList<hif::Action> &actionList,
        hif::BList<hif::Declaration> *declList = nullptr);

    void _buildActionListFromAnalogStatement(analog_statement_t *statement, hif::BList<hif::Action> &actionList);

    static void
    _buildSensitivityFromEventControl(event_control_t *ect, hif::BList<hif::Value> &sensitivity, bool &allSignals);

    void _buildSensitivityFromAnalogEventControl(
        analog_event_control_t *ect,
        hif::BList<hif::Value> &sensitivity,
        bool &allSignals);

    void _manageModuleItemDeclarations(
        hif::BaseContents *contents_o,
        hif::DesignUnit *du,
        module_or_generate_item_declaration_t *decls);

    auto _composeAmsType(hif::Type *portType, hif::Type *declarationType) -> hif::Type *;

    auto _makeValueFromFilter(analog_filter_function_arg_t *arg) -> hif::Value *;
};

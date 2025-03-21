/// @file verilog_parser_struct.cpp
/// @brief
/// Copyright (c) 2024-2025, Electronic Systems Design (ESD) Group,
/// Univeristy of Verona.
/// This file is distributed under the BSD 2-Clause License.
/// See LICENSE.md for details.

#include "verilog2hif/parser_struct.hpp"

auto statement_t::getFirstObject() const -> hif::Object *
{
    if (blocking_assignment != nullptr) {
        return blocking_assignment;
    }
    if (case_statement != nullptr) {
        return case_statement;
    }
    if (conditional_statement != nullptr) {
        return conditional_statement;
    }
    if (disable_statement != nullptr) {
        return disable_statement;
    }
    if (event_trigger != nullptr) {
        return event_trigger;
    }
    if (loop_statement != nullptr) {
        return loop_statement;
    }
    if (nonblocking_assignment != nullptr) {
        return nonblocking_assignment;
    }
    if (system_task_enable != nullptr) {
        return system_task_enable;
    }
    if (task_enable != nullptr) {
        return task_enable;
    }
    if (wait_statement != nullptr) {
        return wait_statement;
    }

    if (seq_block_actions != nullptr) {
        for (auto *c : *seq_block_actions) {
            if (c != nullptr && c->getFirstObject() != nullptr) {
                return c->getFirstObject();
            }
        }
    }

    if (procedural_timing_control != nullptr && procedural_timing_control->getFirstObject() != nullptr) {
        return procedural_timing_control->getFirstObject();
    }
    if (seq_block_declarations != nullptr && !seq_block_declarations->empty()) {
        return seq_block_declarations->front();
    }

    return nullptr;
}

auto block_item_declaration_t::getFirstObject() const -> hif::Object *
{
    if (local_parameter_declaration != nullptr && !local_parameter_declaration->empty()) {
        return local_parameter_declaration->front();
    }
    if (variable_declaration != nullptr && !variable_declaration->empty()) {
        return variable_declaration->front();
    }
    if (reg_variable_declaration != nullptr && !reg_variable_declaration->empty()) {
        return reg_variable_declaration->front();
    }
    if (integer_variable_declaration != nullptr && !integer_variable_declaration->empty()) {
        return integer_variable_declaration->front();
    }

    return nullptr;
}

auto procedural_timing_control_t::getFirstObject() const -> hif::Object *
{
    if (delay_control != nullptr) {
        return delay_control;
    }
    if (event_control != nullptr && event_control->getFirstObject() != nullptr) {
        return event_control->getFirstObject();
    }

    return nullptr;
}

event_control_t::event_control_t()
    : event_identifier(nullptr)
    , event_expression_list(nullptr)
    , event_all(false)
{
}

event_control_t::~event_control_t() = default;

event_control_t::event_control_t(const event_control_t &o)

    = default;

auto event_control_t::operator=(const event_control_t &o) -> event_control_t &
{
    if (this == &o) {
        return *this;
    }

    event_identifier      = o.event_identifier;
    event_expression_list = o.event_expression_list;
    event_all             = o.event_all;

    return *this;
}

auto event_control_t::getFirstObject() const -> hif::Object *
{
    if (event_identifier != nullptr) {
        return event_identifier;
    }

    if (event_expression_list != nullptr) {
        for (auto &i : *event_expression_list) {
            if (i != nullptr && i->getFirstObject() != nullptr) {
                return i->getFirstObject();
            }
        }
    }

    return nullptr;
}

auto event_expression_t::getFirstObject() const -> hif::Object *
{
    if (expression != nullptr) {
        return expression;
    }
    if (posedgeExpression != nullptr) {
        return posedgeExpression;
    }
    if (negedgeExpression != nullptr) {
        return negedgeExpression;
    }

    return nullptr;
}

auto analog_event_expression_t::getFirstObject() const -> hif::Object *
{
    if (or_analog_event_expression != nullptr && !or_analog_event_expression->empty()) {
        return or_analog_event_expression->front();
    }
    if (analysis_identifier_list != nullptr) {
        messageError("Unsupported case", nullptr, nullptr);
    }

    return event_expression_t::getFirstObject();
}

auto analog_event_control_t::getFirstObject() const -> hif::Object *
{
    if (analog_event_expression != nullptr && analog_event_expression->getFirstObject() != nullptr) {
        return analog_event_expression->getFirstObject();
    }
    return event_control_t::getFirstObject();
}
/// @file vhdl_parser_struct.hpp
/// @brief This file contains the definition of the structures used by the
/// vhdl parser. The structures are used to build the HIF tree.
/// Copyright (c) 2024-2025, Electronic Systems Design (ESD) Group,
/// Univeristy of Verona.
/// This file is distributed under the BSD 2-Clause License.
/// See LICENSE.md for details.

#pragma once

#include <hif/hif.hpp>

#include <cstring>

// DESIGN UNIT / VIEW / LIBRARY
struct primary_unit_t;
struct architecture_body_t;
struct secondary_unit_t;
struct library_unit_t;
struct context_item_t;

// STATEMENTS / DECLARATIONS
struct concurrent_statement_t;
struct constraint_t;
struct interface_declaration_t;
struct block_declarative_item_t;
struct block_header_t;
struct entity_declarative_item_t;

// CONFIGURATIONS
struct component_specification_t;
struct component_configuration_t;
struct configuration_item_t;
struct block_specification_t;
struct block_configuration_t;
struct configuration_map_key_t;
struct entity_aspect_t;
struct binding_indication_t;
struct instantiation_list_t;

// PSL
struct assert_directive_t;
struct verification_directive_t;

/// @brief Data about a identifier.
struct identifier_data_t {
    int line;   ///< The source code line number.
    int column; ///< The source code column number.
    int len;    ///< The length of the token.
    char *name; ///< The name of the token.
};

/// @brief Data about a keyword.
struct keyword_data_t {
    int line;   ///< The source code line number.
    int column; ///< The source code column number.
};

/* -----------------------------------------------------------------------
 *  VHDL
 * ----------------------------------------------------------------------- */

struct concurrent_statement_t {
    hif::StateTable *process;
    // .. procedure_call
    // .. assertion
    hif::BList<hif::Assign> *signal_assignment;
    hif::Instance *instantiation;
    hif::Generate *generate;
    hif::Instance *component_instantiation;
    hif::View *block;
};

struct primary_unit_t {
    hif::DesignUnit *entity_declaration;
    hif::LibraryDef *package_declaration;
    // ..
};

struct architecture_body_t {
    architecture_body_t()
        : components()
    {
        // ntd
    }

    ~architecture_body_t()
    {
        delete entity_name;
        // do not delete 'contents'
    }

    architecture_body_t(const architecture_body_t &o)
        : contents(o.contents)
        , entity_name(o.entity_name)
        , components(o.components)
    {
        // ntd
    }

    auto operator=(const architecture_body_t &o) -> architecture_body_t &
    {
        if (this == &o) {
            contents = o.contents;
        }
        entity_name = o.entity_name;
        components  = o.components;
        return *this;
    }

    hif::Contents *contents{nullptr};
    hif::Identifier *entity_name{nullptr};
    std::list<hif::DesignUnit *> components;
};

struct secondary_unit_t {
    architecture_body_t *architecture_body;
    hif::LibraryDef *package_body;
};

struct library_unit_t {
    primary_unit_t *primary_unit;
    secondary_unit_t *secondary_unit;
};

struct context_item_t {
    hif::BList<hif::Library> *library_clause;
    hif::BList<hif::Library> *use_clause;
};

struct constraint_t {
    hif::Range *range_constraint;
    hif::BList<hif::Range> *index_constraint;
};

struct interface_declaration_t {
    hif::BList<hif::Port> *port_list;
    hif::BList<hif::Port> *signal_declaration;
    hif::BList<hif::Port> *constant_declaration;
    hif::BList<hif::Port> *variable_declaration;
};

struct block_declarative_item_t {
    hif::BList<hif::Declaration> *declarations;
    hif::BList<hif::Library> *use_clause;
    component_configuration_t *configuration_specification;
    hif::DesignUnit *component_declaration;
    bool isSkipped;
};

struct block_header_t {
    hif::BList<hif::Declaration> *block_header_generic_part;
    hif::BList<hif::Port> *block_header_port_part;
};

struct entity_declarative_item_t {
    hif::BList<hif::Declaration> *constant_declaration;
    hif::BList<hif::Declaration> *variable_declaration;
    hif::BList<hif::Declaration> *signal_declaration;
    hif::Alias *alias_declaration;
    hif::TypeDef *subtype_declaration;
    hif::TypeDef *type_declaration;
    hif::BList<hif::Library> *use_clause;
    hif::Declaration *subprogram_declaration;
    hif::Declaration *subprogram_body;
    bool isSkipped;
};

struct subtype_indication_t {
    hif::Value *value;
    hif::Type *type;
    hif::Range *range;
};

/* -----------------------------------------------------------------------
 *  VHDL CONFIGURATION STATEMENTS
 * ----------------------------------------------------------------------- */

/// @brief Identifies a particular design entity to be associated with instances
/// of a component.
struct entity_aspect_t {
    hif::ViewReference *entity;
    hif::Identifier *configuration;
};

/// @brief Associates instances of a component with a particular design entity.
/// It may also associate actuals with formals declared in the entity declaration.
struct binding_indication_t {
    // entity_aspect
    hif::BList<hif::TPAssign> *generic_map_aspect;
    hif::BList<hif::PortAssign> *port_map_aspect;
    entity_aspect_t entity_aspect;
};

/// @brief The component instances to which this component configuration applies.
struct instantiation_list_t {
    instantiation_list_t()

    {
        // do nothing
    }

    // Mutually exclusive:

    /// @brief List of component instances.
    hif::BList<hif::Identifier> *identifier_list{nullptr};

    /// @brief Field indicating all unspecified instances.
    bool others{false};

    /// @brief Field indicating all instances.
    bool all{false};
};

/// @brief Identifies the component instances to which this component
/// configuration applies.
struct component_specification_t {
    // Mutually exclusive:

    /// @brief Multiple instances.
    instantiation_list_t *instantiation_list;

    /// @brief Single instance.
    hif::Identifier *component_name;
};

/// @brief Defines the configuration of one or more component instances in a
/// corresponding block.
struct component_configuration_t {
    /// @brief One (or list of) instance.
    component_specification_t *component_specification;

    /// @brief Binding indication.
    binding_indication_t *binding_indication;
};

struct configuration_item_t {
    block_configuration_t *block_configuration;
    component_configuration_t *component_configuration;
};

struct block_specification_t {
    hif::Identifier *block_name;
    // ...
};

/// @brief Architecture-related map: stores binding indication for the instances
/// contained in an architecture.
struct block_configuration_t {
    /// @brief The architecture containing the instances.
    block_specification_t *block_specification;

    /// @brief For any instance store the entity/configuration to use.
    std::map<hif::Instance *, binding_indication_t *> *component_configuration_map;
};

/// @brief Configuration-related-map key, used to univocally identify a configuration.
struct configuration_map_key_t {
    /// @brief The name of entity in which the configuration is specified.
    std::string design_unit;

    /// @brief The name of the configuration.
    std::string configuration;

    /// @brief The name of the architecture for which the configuration is
    /// specified.
    // Note: by now, only architecture is supported.
    std::string view;

    auto operator<(const configuration_map_key_t &other) const -> bool
    {
        if (this == &other) {
            return false;
        }

        if (design_unit < other.design_unit) {
            return true;
        }
        if (configuration < other.configuration) {
            return true;
        }
        if (view < other.view) {
            return true;
        }

        return false;
    }
};

/* -----------------------------------------------------------------------
 *  PSL
 * ----------------------------------------------------------------------- */

struct vunit_item_t {
    hif::ProcedureCall *psl_directive;
    // psl_declaration
    // vunit_instance
    block_declarative_item_t *hdl_decl;
    concurrent_statement_t *concurrent_statement;
};

struct assert_directive_t {
    hif::Value *property;
    hif::Value *report;
    hif::Value *severity;
};

struct verification_directive_t {
    // mutually exclusive
    assert_directive_t *assert_directive;
    // assume_directive
    // restrict_directive
    // restrict_excl_directive
    // cover_directive
    // fairness_statement

    // common identifier
    hif::Identifier *label;
};

/// @file VhdlParser_extension.cpp
/// @brief
/// Copyright (c) 2024-2025, Electronic Systems Design (ESD) Group,
/// Univeristy of Verona.
/// This file is distributed under the BSD 2-Clause License.
/// See LICENSE.md for details.

#include <map>
#include <utility>

#include "vhdl2hif/vhdl_parser.hpp"
#include "vhdl2hif/vhdl_support.hpp"

using namespace hif;
using std::list;
using std::make_pair;
using std::map;
using std::pair;
using std::string;

namespace /*anon*/
{

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunknown-pragmas"
#elif defined __GNUC__
#pragma GCC diagnostic ignored "-Wunsafe-loop-optimizations"
#endif

const pair<string, string> operator_overloading[] = {
    pair<string, string>("or", "__vhdl_op_bor"), pair<string, string>("and", "__vhdl_op_band"),
    pair<string, string>("xor", "__vhdl_op_bxor"), pair<string, string>("not", "__vhdl_op_bnot"),
    pair<string, string>("=", "__vhdl_op_eq"), pair<string, string>("/=", "__vhdl_op_neq"),
    pair<string, string>("<=", "__vhdl_op_le"), pair<string, string>(">=", "__vhdl_op_ge"),
    pair<string, string>("<", "__vhdl_op_lt"), pair<string, string>(">", "__vhdl_op_gt"),
    pair<string, string>("sll", "__vhdl_op_sll"), pair<string, string>("srl", "__vhdl_op_srl"),
    pair<string, string>("sla", "__vhdl_op_sla"), pair<string, string>("sra", "__vhdl_op_sra"),
    pair<string, string>("rol", "__vhdl_op_rol"), pair<string, string>("ror", "__vhdl_op_ror"),
    pair<string, string>("+", "__vhdl_op_plus"), pair<string, string>("-", "__vhdl_op_minus"),
    pair<string, string>("*", "__vhdl_op_mult"), pair<string, string>("/", "__vhdl_op_div"),
    pair<string, string>("mod", "__vhdl_op_mod"), pair<string, string>("rem", "__vhdl_op_rem"),
    pair<string, string>("**", "__vhdl_op_pow"), pair<string, string>("abs", "__vhdl_op_abs"),
    pair<string, string>("&", "__vhdl_op_concat"),

    // supported?
    pair<string, string>("nor", "__vhdl_op_nor"), pair<string, string>("nand", "__vhdl_op_nand"),
    pair<string, string>("xnor", "__vhdl_op_xnor")};

const string std_vhdl_types[] = {
    //std_types
    "bit", "bit_vector", "boolean", "character", "integer", "natural", "positive", "real", "string", "time",
    //with std_logic_1164
    "std_ulogic", "std_ulogic_vector", "std_logic", "std_logic_vector", "x01", "x01z", "ux01", "ux01z",
    //with numeric_std
    "unsigned", "signed", "unresolved_unsigned", "u_unsigned", "unresolved_signed", "u_signed",
    //with std_logic_arith
    "small_int",
    //with std_logic_misc
    "strength", "minomax",
    //with std_textio
    "line", "text", "width", "side"};

} // namespace

void VhdlParser::_initStandardLibraries()
{
    _is_vhdl_type =
        new std::set<std::string>(std_vhdl_types, (std_vhdl_types + (sizeof(std_vhdl_types) / sizeof(string))));

    _is_operator_overloading = new map<string, string>(
        operator_overloading,
        (operator_overloading + (sizeof(operator_overloading) / sizeof(operator_overloading[0]))));
}

void VhdlParser::_populateContents(Contents *contents_o, std::list<concurrent_statement_t *> *concurrent_statement_list)
{
    GlobalAction *global_o = contents_o->getGlobalAction();
    for (auto &i : *concurrent_statement_list) {
        concurrent_statement_t *stm = i;

        if (stm->process != nullptr) {
            contents_o->stateTables.push_back(i->process);
        } else if (stm->instantiation != nullptr) {
            contents_o->instances.push_back(i->instantiation);
        } else if (stm->signal_assignment != nullptr) {
            global_o->actions.merge(*i->signal_assignment);
            delete i->signal_assignment;
        } else if (stm->generate != nullptr) {
            contents_o->generates.push_back(i->generate);
        } else if (stm->component_instantiation != nullptr) {
            contents_o->instances.push_back(i->component_instantiation);
        } else if (stm->block != nullptr) {
            contents_o->declarations.push_back(stm->block);
        } else {
            yyerror("Unexpected architecture concurrent statement.");
        }

        delete i;
    }
}

void VhdlParser::_populateConfigurationMap(
    configuration_item_t *configuration_item,
    component_configuration_map_t *config_map)
{
    if (configuration_item->component_configuration != nullptr) {
        component_configuration_t *comp_config = configuration_item->component_configuration;
        component_specification_t *comp_spec   = comp_config->component_specification;

        ViewReference *viewref_o = nullptr;

        if (comp_spec->instantiation_list->identifier_list != nullptr) {
            BList<Identifier> *id_list = comp_spec->instantiation_list->identifier_list;

            for (BList<Identifier>::iterator id = id_list->begin(); id != id_list->end(); ++id) {
                viewref_o = new ViewReference();
                setCodeInfo(viewref_o);
                viewref_o->setDesignUnit(comp_spec->component_name->getName());

                auto *instance_o = new Instance();
                setCodeInfo(instance_o);
                instance_o->setName((*id)->getName());
                instance_o->setReferencedType(viewref_o);

                config_map->insert(make_pair(instance_o, comp_config->binding_indication));
            }

            delete id_list;
        } else if (comp_spec->instantiation_list->all) {
            viewref_o = new ViewReference();
            setCodeInfo(viewref_o);
            viewref_o->setDesignUnit(comp_spec->component_name->getName());

            auto *instance_o = new Instance();
            setCodeInfo(instance_o);
            instance_o->setReferencedType(viewref_o);
            instance_o->addProperty("CONFIGURATION_ALL");

            config_map->insert(make_pair(instance_o, comp_config->binding_indication));
        } else if (comp_spec->instantiation_list->others) {
            viewref_o = new ViewReference();
            setCodeInfo(viewref_o);
            viewref_o->setDesignUnit(comp_spec->component_name->getName());

            auto *instance_o = new Instance();
            setCodeInfo(instance_o);
            instance_o->setReferencedType(viewref_o);
            instance_o->addProperty("CONFIGURATION_OTHERS");

            config_map->insert(make_pair(instance_o, comp_config->binding_indication));
        }

        delete comp_spec->component_name;
        delete comp_spec->instantiation_list;
        delete comp_spec;
    } else if (configuration_item->block_configuration != nullptr) {
        block_configuration_t *block_config = configuration_item->block_configuration;
        map<Instance *, binding_indication_t *> *component_configuration_map =
            block_config->component_configuration_map;

        for (auto &it : *component_configuration_map) {
            // TODO: manage block_name

            //            ViewReference * view_ref =
            //                    dynamic_cast<ViewReference*>( it->first->getReferencedType() );
            //
            //            Identifier * block_name = block_config->block_specification->block_name;
            //            view_ref->setName(  block_name->getName( ) );

            config_map->insert(make_pair(it.first, it.second));
        }

        component_configuration_map->clear();

        delete block_config->block_specification;
        delete block_config->component_configuration_map;
    } else {
        messageDebugAssert(false, "Unexpected case", nullptr, _sem);
    }
}

auto VhdlParser::_resolveType(string type_ref, BList<Value> *opt_arg, Range *ro) -> Type *
{
    Type *t = resolveType(std::move(type_ref), opt_arg, ro, _sem, true);
    if (t != nullptr) {
        _factory.codeInfo(t, yyfilename, static_cast<unsigned int>(yylineno));
    }

    return t;
}

auto VhdlParser::_resolveFieldReferenceType(FieldReference *fr) -> ReferencedType *
{
    auto *ret = new TypeReference();
    setCodeInfo(ret);
    ret->setName(fr->getName());
    ret->setInstance(resolveLibraryType(fr->getPrefix()));
    _factory.codeInfo(ret, ret->getSourceFileName(), ret->getSourceLineNumber());

    return ret;
}

auto VhdlParser::buildSystemObject() -> System *
{
    auto *system_o = new System();
    system_o->setName("system");

    for (auto *archBody : *du_definitions) {
        messageAssert(archBody != nullptr, "Unexpected nullptr", nullptr, nullptr);

        for (BList<DesignUnit>::iterator dui = du_declarations->begin(); dui != du_declarations->end(); ++dui) {
            if ((*dui)->getName() == archBody->entity_name->getName()) {
                Contents *old = (*dui)->views.back()->setContents(archBody->contents);
                messageAssert(old == nullptr, "Multiple architectures are not supported yet", *dui, nullptr);
                (*dui)->views.back()->setName(archBody->contents->getName());
                (*dui)->views.back()->setStandard(false);
                break;
            }
        }

        for (auto *comp : archBody->components) {
            bool found = false;
            for (BList<DesignUnit>::iterator dui = du_declarations->begin(); dui != du_declarations->end(); ++dui) {
                if ((*dui)->getName() == comp->getName()) {
                    found = true;
                    break;
                }
            }

            if (!found) {
                du_declarations->push_back(comp);
            }
        }

        delete archBody;
    }
    system_o->designUnits.merge(*du_declarations);

    for (BList<LibraryDef>::iterator libi = lib_declarations->begin(); libi != lib_declarations->end(); ++libi) {
        for (BList<LibraryDef>::iterator packi = lib_definitions->begin(); packi != lib_definitions->end(); ++packi) {
            if ((*libi)->getName() == (*packi)->getName()) {
                (*libi)->libraries.merge((*packi)->libraries);
                (*libi)->declarations.merge((*packi)->declarations);
                break;
            }
        }
    }
    system_o->libraryDefs.merge(*lib_declarations);

    delete du_definitions;
    delete lib_definitions;
    delete du_declarations;
    delete lib_declarations;

    return system_o;
}

void VhdlParser::_fixIntancesWithComponent(Contents *contents, View *component)
{
    hif::HifTypedQuery<Instance> q;
    std::list<Instance *> list;
    hif::search(list, contents, q);

    for (auto *inst : list) {
        if (dynamic_cast<BaseContents *>(inst->getParent()) == nullptr) {
            // In this case it could be a prefix of a fcall:
            // e.g. HifUtility standard functions
            continue;
        }

        auto *vr = dynamic_cast<ViewReference *>(inst->getReferencedType());
        messageAssert(vr != nullptr, "Unexpected ReferencedType.", inst->getReferencedType(), nullptr);

        auto *du = dynamic_cast<DesignUnit *>(component->getParent());
        messageAssert(du != nullptr, "Cannot find parent du.", vr, nullptr);

        if ((vr->getName() != hif::NameTable::none() && vr->getName() != component->getName()) ||
            du->getName() != vr->getDesignUnit()) {
            continue;
        }

        // Fix templates
        for (BList<Declaration>::iterator j = component->templateParameters.begin();
             j != component->templateParameters.end(); ++j) {
            auto *vtp = dynamic_cast<ValueTP *>(*j);
            messageAssert(vtp != nullptr, "Unexpected Template.", *j, nullptr);

            bool found = false;
            for (BList<TPAssign>::iterator k = vr->templateParameterAssigns.begin();
                 k != vr->templateParameterAssigns.end(); ++k) {
                auto *vtpa = dynamic_cast<ValueTPAssign *>(*k);
                messageAssert(vtpa != nullptr, "Unexpected template assign.", *k, _sem);

                messageAssert(!vtpa->getName().empty(), "Unexpected nullptr name", vtpa, _sem);
                if (vtpa->getName() == hif::NameTable::none()) {
                    vtpa->setName(vtp->getName());
                }

                if (vtp->getName() != vtpa->getName()) {
                    continue;
                }
                found = true;
                messageAssert(vtpa->getValue() != nullptr, "Unexpected nullptr value", vtpa, _sem);
                break;
            }

            if (vtp->getValue() == nullptr) {
                continue;
            }
            if (!found) {
                auto *vtpa = new ValueTPAssign();
                vtpa->setValue(hif::copy(vtp->getValue()));
                vtpa->setName(vtp->getName());
                vr->templateParameterAssigns.push_back(vtpa);
            }
        }

        // Fix ports
        for (BList<Port>::iterator j = component->getEntity()->ports.begin(); j != component->getEntity()->ports.end();
             ++j) {
            Port *port = *j;

            bool found = false;
            for (BList<PortAssign>::iterator k = inst->portAssigns.begin(); k != inst->portAssigns.end(); ++k) {
                PortAssign *pa = *k;

                messageAssert(!pa->getName().empty(), "Unexpected nullptr name", pa, _sem);
                if (pa->getName() == hif::NameTable::none()) {
                    pa->setName(port->getName());
                }

                if (port->getName() != pa->getName()) {
                    continue;
                }
                found = true;
                // Can be an explicit open oprt assign...
                //assert (pa->getValue() != nullptr);
                break;
            }

            if (port->getValue() == nullptr) {
                continue;
            }
            if (!found) {
                auto *pa = new PortAssign();
                pa->setValue(hif::copy(port->getValue()));
                pa->setName(port->getName());
                inst->portAssigns.push_back(pa);
            }
        }
    }
}

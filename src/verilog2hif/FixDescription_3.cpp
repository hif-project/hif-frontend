/// @file FixDescription_3.cpp
/// @brief
/// Copyright (c) 2024-2025, Electronic Systems Design (ESD) Group,
/// Univeristy of Verona.
/// This file is distributed under the BSD 2-Clause License.
/// See LICENSE.md for details.

#include <cstdlib>
#include <iostream>

#include <hif/hif.hpp>

#include "verilog2hif/post_parsing_methods.hpp"
#include "verilog2hif/support.hpp"

#ifndef NDEBUG
//#define DBG_PRINT_FIX3_STEP_FILES
#endif

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-member-function"
#elif defined __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#endif

using namespace hif;

// ///////////////////////////////////////////////////////////////////
// Support
// ///////////////////////////////////////////////////////////////////
using Views            = hif::manipulation::ViewSet;
using ViewRefs         = std::set<ViewReference *>;
using DataDeclarations = std::set<DataDeclaration *>;

auto getBaseContents(Declaration *context) -> BaseContents *
{
    Port *p = dynamic_cast<Port *>(context);
    if (p != nullptr) {
        View *v = hif::getNearestParent<View>(p);
        messageAssert(v != nullptr, "Cannot find parent view", p, nullptr);
        messageAssert(v->getContents() != nullptr, "Contents not found.", p, nullptr);
        return v->getContents();
    }

    auto *bc = getNearestParent<BaseContents>(context);
    messageAssert(bc != nullptr, "Contents not found.", p, nullptr);
    return bc;
}

auto skipStandardFunctionCall(Object *o, const hif::HifQueryBase *q) -> bool
{
    auto *fc = dynamic_cast<FunctionCall *>(o);
    if (fc == nullptr) {
        return false;
    }

    Function *foo = hif::semantics::getDeclaration(fc, q->sem);
    if (foo == nullptr) {
        return true;
    }

    return !hif::declarationIsPartOfStandard(foo);
}

// ///////////////////////////////////////////////////////////////////
// prerefine fixes
// ///////////////////////////////////////////////////////////////////

using RefMap = hif::semantics::ReferencesMap;
using RefSet = hif::semantics::ReferencesSet;

void fixUnreferenced(RefMap &refMap, hif::semantics::ILanguageSemantics * /*sem*/)
{
    RefSet trash;
    for (auto i = refMap.begin(); i != refMap.end(); ++i) {
        auto *sig = dynamic_cast<Signal *>(i->first);

        if (sig == nullptr || !i->second.empty()) {
            continue;
        }

        // sig w/o refs is refined to var
        auto *var = new Variable();
        var->setName(sig->getName());
        var->setType(sig->setType(nullptr));
        var->setValue(sig->setValue(nullptr));

        sig->replace(var);
        trash.insert(sig);
        refMap[var];
    }

    // clean signals moved to variables.
    for (auto *i : trash) {
        refMap.erase(dynamic_cast<Declaration *>(i));
        delete i;
    }
}

auto checkSubNodeOfTop(Port *p, Views &topViews) -> bool
{
    for (auto *top : topViews) {
        if (hif::isSubNode(p, top)) {
            return true;
        }
    }
    return false;
}

void fixOutputPorts(Views &topViews, RefMap &refMap, hif::semantics::ILanguageSemantics *sem)
{
    hif::HifFactory factory(sem);

    // Output ports assigned by continuous assignments are replaced with explicit
    // processes.
    hif::application_utils::WarningList fixed;
    for (auto i = refMap.begin(); i != refMap.end(); ++i) {
        Port *p = dynamic_cast<Port *>(i->first);
        if (p == nullptr) {
            continue;
        }
        if (p->getDirection() != dir_out && p->getDirection() != dir_inout) {
            continue;
        }
        bool inTopView = checkSubNodeOfTop(p, topViews);
        RefSet &portRefs     = i->second;

        RefSet continuousTargets;
        RefSet readRefs;
        RefSet nonblockingSet;
        RefSet portassSet;
        for (auto *symb : portRefs) {
            auto *portAss = dynamic_cast<PortAssign *>(symb);
            if (portAss != nullptr) {
                portassSet.insert(symb);
                continue;
            }

            bool isRead = !hif::manipulation::isInLeftHandSide(symb);
            if (isRead) {
                readRefs.insert(symb);
                continue;
            }

            auto *ass = hif::getNearestParent<Assign>(symb);
            messageAssert(ass != nullptr, "Cannot find parent assign", symb, sem);

            auto *gact                  = hif::getNearestParent<GlobalAction>(symb);
            bool isBlockingAssign = gact != nullptr || !ass->checkProperty(NONBLOCKING_ASSIGNMENT);

            if (isBlockingAssign) {
                continuousTargets.insert(symb);
            } else {
                nonblockingSet.insert(symb);
            }
        }

        // Assigned only by non-blocking.
        // Possible reads to the output port will see a delta-cycle delay,
        // as in Verilog.
        if (continuousTargets.empty()) {
            continue;
        }

        // Ref designs:
        // - openCores/or1200_immu (submodule of or1200_toop)
        // - openCores/i2cSlaveTop
        if (readRefs.empty()) {
            for (auto *symb : continuousTargets) {
                auto *ass = hif::getNearestParent<Assign>(symb);
                if (!inTopView) {
                    fixed.push_back(ass);
                }
                ass->addProperty(NONBLOCKING_ASSIGNMENT);
                auto *gact = hif::getNearestParent<GlobalAction>(symb);
                if (gact == nullptr) {
                    continue;
                }
                std::set<StateTable *> list;
                hif::manipulation::transformGlobalActions(ass, list, sem);
                // Update references
                for (auto *st : list) {
                    // Ref design: openCores/log2
                    //st->setDontInitialize(true);
                    hif::semantics::getAllReferences(refMap, sem, st);
                }
            }
        } else {
            // There are some reads, and also some blocking/continuous assignments.
            // Replacing all refs with a new signal, and adding a new process to
            // update the output port.
            // Ref design: openCores/or1200_top
            auto *sig = new Signal();
            sig->setName(NameTable::getInstance()->getFreshName(p->getName(), "_out_sig"));
            sig->setType(hif::copy(p->getType()));
            sig->setValue(hif::copy(p->getValue()));
            hif::manipulation::addDeclarationInContext(sig, p);

            RefSet &sigRefs = refMap[sig];
            for (auto *symb : continuousTargets) {
                hif::objectSetName(symb, sig->getName());
                hif::semantics::setDeclaration(symb, sig);
                sigRefs.insert(symb);
            }
            for (auto *symb : readRefs) {
                hif::objectSetName(symb, sig->getName());
                hif::semantics::setDeclaration(symb, sig);
                sigRefs.insert(symb);
            }
            for (auto *symb : nonblockingSet) {
                hif::objectSetName(symb, sig->getName());
                hif::semantics::setDeclaration(symb, sig);
                sigRefs.insert(symb);
            }

            // Updating ref map
            portRefs.clear();
            portRefs.insert(portassSet.begin(), portassSet.end());

            // Adding process to update output port:
            Assign *ass = factory.assignment(new Identifier(p->getName()), new Identifier(sig->getName()));
            ass->addProperty(NONBLOCKING_ASSIGNMENT);
            StateTable *st = factory.stateTable(
                (p->getName() + std::string("_update_process")), factory.noDeclarations(), ass,
                false // Ref design: openCores/i2cSlaveTop
            );
            st->sensitivity.push_back(new Identifier(sig->getName()));
            hif::manipulation::addDeclarationInContext(st, p);
            hif::semantics::getAllReferences(refMap, sem, st);
        }
    }

    messageWarningList(
        true,
        "Found at least one output port assigned by continuous"
        " assignment. It will be replaced with a process."
        " This could lead to a non-equivalent output design description.",
        fixed);
}

void performChecks(RefMap &refMap, hif::semantics::ILanguageSemantics *sem)
{
    hif::application_utils::WarningSet fixedParams;
    hif::application_utils::WarningSet fixedVars;

    for (auto &i : refMap) {
        if (dynamic_cast<Parameter *>(i.first) != nullptr) {
            // Parameters cannot be signal (are always temporaries)
            // So they must be assign as blocking.
            auto *p        = dynamic_cast<Parameter *>(i.first);
            RefSet &refSet = i.second;
            for (auto j = refSet.begin(); j != refSet.end(); ++j) {
                if (!hif::manipulation::isInLeftHandSide(*j)) {
                    continue;
                }

                auto *ass = hif::getNearestParent<Assign>(*j);
                messageAssert(ass != nullptr, "Cannot find assign", *j, sem);

                if (ass->checkProperty(NONBLOCKING_ASSIGNMENT)) {
                    ass->removeProperty(NONBLOCKING_ASSIGNMENT);
                    fixedParams.insert(p);
                }
            }
        } else if (dynamic_cast<Variable *>(i.first) != nullptr) {
            // variables of automatic task or function cannot be signals.
            // So they must be assign as blocking.
            auto *var      = dynamic_cast<Variable *>(i.first);
            RefSet &refSet = i.second;
            for (auto j = refSet.begin(); j != refSet.end(); ++j) {
                if (!hif::manipulation::isInLeftHandSide(*j)) {
                    continue;
                }

                auto *ass = hif::getNearestParent<Assign>(*j);
                messageAssert(ass != nullptr, "Cannot find assign", *j, sem);

                if (ass->checkProperty(NONBLOCKING_ASSIGNMENT)) {
                    ass->removeProperty(NONBLOCKING_ASSIGNMENT);
                    fixedVars.insert(var);
                }
            }
        }
    }

    messageWarningList(
        true,
        "Found at least one task or function parameter assigned through a"
        " non-blocking assignment. The assignment will be translated as "
        "blocking."
        " This could lead to a non-equivalent output design description.",
        fixedParams);

    messageWarningList(
        true,
        "Found at least one task or function internal variable assigned "
        "through a"
        " non-blocking assignment. The assignment will be translated as "
        "blocking."
        " This could lead to a non-equivalent output design description.",
        fixedVars);
}

void prerefineFixes(Views &topViews, RefMap &refMap, hif::semantics::ILanguageSemantics *sem)
{
    // Unreferenced signal can be safetely translated as variables.
    fixUnreferenced(refMap, sem);

    // Output ports assigned by continuous assignments are replaced with explicit
    // processes. If found, a warnings is raised.
    fixOutputPorts(topViews, refMap, sem);

    // Checks:
    // - calls parameters. They cannot be signal (are always temporaries).
    //   So they must be assign as blocking.
    // - variables of automatic task or function. They cannot be signals.
    //   So they must be assign as blocking.
    performChecks(refMap, sem);
}

// /////////////////////////////////////////////////////////////////////////////
// _expandForGenerates
// /////////////////////////////////////////////////////////////////////////////

/// @brief Replicates every `for generate` in place, one copy per iteration.
///
/// Everything after this point in step 3 - splitLogicConesLoops,
/// generateConeFunctions, addConesPCalls - assumes that a continuous assignment
/// is valid in the scope of the declaration it drives. A ForGenerate breaks that
/// assumption: generateConeFunctions places the cone beside the driven
/// declaration and fills it with copies of the driver assignments, so a driver
/// living inside the loop is copied out of the loop, taking its references to
/// loop-scoped names with it. Those are the genvar, and - since hif-frontend#23
/// declared it there - the split signal. Neither resolves where the cone lands,
/// and standardization aborts with "Declaration not found" naming the cone.
///
/// The cone cannot follow the assignment into the loop instead: its callers are
/// the readers of the driven declaration, which sit at module scope, so
/// addConesPCalls would emit a call that does not resolve. Passing the genvar as
/// a cone parameter does not work either, because a module-scope reader has no
/// index to pass and needs every iteration run. Expanding the loop is what
/// removes the mismatch rather than moving it.
///
/// Only ForGenerate is expanded. An IfGenerate is left alone: it is not
/// replicated and declares no index, so it produces neither loop-scoped name,
/// and expanding it would change output for designs that have a different
/// defect behind them (hif-frontend#32, hif-backend#86).
/// @return True when at least one loop was expanded, so the caller knows the
///         reference map and types need rebuilding.
auto expandForGenerates(System *system, hif::semantics::ILanguageSemantics *sem, bool preserveStructure) -> bool
{
    // Expansion is only safe once instances have been specialized, and
    // partialFlattening - which is what specializes them - does nothing under
    // preserveStructure. Expanding anyway would resolve a parameterized loop
    // bound against the module's *default*: a leaf declaring `parameter N = 2`
    // and instantiated as `#(.N(4))` elaborates two iterations instead of four,
    // at exit 0. Measured, not assumed.
    //
    // So under -s the ForGenerate is left standing, which is also what the flag
    // asks for, and a design that needs a cone across it keeps the abort it
    // already has today. That is the same trade as the unit-step test below:
    // a loud failure in preference to a silently different design.
    if (preserveStructure) {
        return false;
    }

    using Query = hif::HifTypedQuery<ForGenerate>;
    Query query;
    query.skipStandardScopes = true;
    Query::Results generates;
    hif::search(generates, system, query);

    // Only the outermost loops. Expanding one replicates its body, and the
    // replication visits the copy, so a nested loop is expanded as part of its
    // parent - and the pointer collected for it above refers to a node that no
    // longer exists by then. The whole set is collected before anything is
    // expanded, so this test never inspects a freed node.
    std::vector<ForGenerate *> outermost;
    for (auto *generate : generates) {
        if (hif::getNearestParent<ForGenerate>(generate) != nullptr) {
            continue;
        }
        outermost.push_back(generate);
    }

    // simplify_constants and simplify_template_parameters are deliberately left
    // off. _simplifyForGenerate turns both on for itself while it resolves the
    // loop bound and restores them afterwards, so the bound is still folded;
    // turning them on here instead would fold them across the whole design and
    // strip the parameterisation the translation is meant to keep - measured on
    // the clog2_replication test, where DEPTH stopped being referenced at all.
    hif::manipulation::SimplifyOptions opt;
    opt.simplify_generates = true;

    for (auto *generate : outermost) {
        // Deliberately the Object* overload. Expanding the loop deletes the
        // ForGenerate, and simplify() returns that freed pointer; the
        // simplify<T>() template then dynamic_casts it, which is a read of
        // freed memory and segfaults here. Casting at the call site picks the
        // overload that hands back an Object* nobody dereferences. hif-core#23.
        hif::manipulation::simplify(static_cast<Object *>(generate), sem, opt);
    }

    return !outermost.empty();
}

// /////////////////////////////////////////////////////////////////////////////
// _splitLogicConesLoops
// /////////////////////////////////////////////////////////////////////////////

auto assignQuery(Object *o, const HifQueryBase * /*query*/) -> bool
{
    // collecting continuous assigns on members and slices
    auto *ass = dynamic_cast<Assign *>(o);
    if (ass == nullptr) {
        return false;
    }
    auto *ga = dynamic_cast<GlobalAction *>(ass->getParent());
    if (ga == nullptr) {
        return false;
    }
    auto *member = dynamic_cast<Member *>(ass->getLeftHandSide());
    auto *slice  = dynamic_cast<Slice *>(ass->getLeftHandSide());
    return slice != nullptr || member != nullptr;
}

auto refMatchesToken(Object *ref, Value *token) -> Value *
{
    Object *last = ref;
    while (last != nullptr) {
        auto *member = dynamic_cast<Member *>(last->getParent());
        auto *slice  = dynamic_cast<Slice *>(last->getParent());
        if (member == nullptr && slice == nullptr) {
            break;
        }
        last = last->getParent();
    }

    if (hif::equals(last, token)) {
        return dynamic_cast<Value *>(last);
    }
    return nullptr;
}

void splitLogicConesLoops(System *system, RefMap &refMap, hif::semantics::ILanguageSemantics *sem)
{
    // Manipulation example:
    // a[0] = expr1
    // a[1] = expr2
    // -->
    // a_0 = expr1
    // a_1 = expr2
    // a[0] = a_0
    // a[1] = a_1
    // This should ensure to split possible loops.
    using Query             = hif::HifTypedQuery<Assign>;
    using ValueSet          = std::set<Value *>;
    using DeclarationTokens = std::map<DataDeclaration *, ValueSet>;
    using TokenNames        = std::map<Value *, std::string>;

    // Get candidate assigns.
    Query query;
    query.check_object_method = &assignQuery;
    query.skipStandardScopes  = true;
    Query::Results assigns;
    hif::search(assigns, system, query);
    // Collect assign targets
    DataDeclarations declarations;
    DeclarationTokens declarationTokens;
    TokenNames tokenNames;
    // Unwrap Member/Slice wrappers down to the underlying Identifier: the
    // TerminalPrefixOptions default constructor value-initializes
    // recurseIntoMembers/recurseIntoSlices to false (despite the header's
    // doc-comment claiming they default to true), so without explicit
    // options getTerminalPrefix would return the Member/Slice itself,
    // which getDeclaration() cannot resolve (it isn't a symbol).
    hif::TerminalPrefixOptions terminalPrefixOptions;
    terminalPrefixOptions.recurseIntoMembers = true;
    terminalPrefixOptions.recurseIntoSlices  = true;
    for (auto *ass : assigns) {
        Value *tgtValue      = ass->getLeftHandSide();
        Value *tgt           = hif::getTerminalPrefix(tgtValue, terminalPrefixOptions);
        Declaration *tgtDecl = hif::semantics::getDeclaration(tgt, sem);
        messageAssert(tgtDecl != nullptr, "Declaration not found", tgt, sem);
        auto *tgtDataDecl = dynamic_cast<DataDeclaration *>(tgtDecl);
        messageAssert(tgtDataDecl != nullptr, "Expected DataDeclaration", tgtDecl, sem);
        declarations.insert(tgtDataDecl);
        ValueSet &valueSet = declarationTokens[tgtDataDecl];
        std::stringstream ss;
        ss << tgtDataDecl->getName() << "_partial" << valueSet.size();
        std::string name = hif::NameTable::getInstance()->getFreshName(ss.str());
        valueSet.insert(tgtValue);
        tokenNames[tgtValue] = name;
    }

    // 1- Replacing all prefixed refs with fresh signals.
    // 2- Generating assigns from tokens to original declaration.
    // 3- Generating new signal declarations
    hif::Trash trash;
    for (auto *decl : declarations) {
        ValueSet &values = declarationTokens[decl];

        // @TODO remove refs from refMap and add tokendecl refs to refMap
        for (auto *token : values) {
            RefSet &refSet        = refMap[decl];
            std::string tokenName = tokenNames[token];

            // The statement being split. The write-back generated below stands
            // for part of it and is attributed to it: without that, a signal
            // with more than one of these locations offers nothing to tell them
            // apart, because the position was the only field that differed
            // (hif-muffin#24).
            //
            // The token is the assign's left-hand side and is never detached
            // before the write-back is inserted, so its parent is that assign;
            // the insertion below already depends on it being an object in a
            // BList. Asserting says so rather than leaving it implied.
            auto *sourceAssign = dynamic_cast<Assign *>(token->getParent());
            messageAssert(sourceAssign != nullptr, "Expected the token's parent to be its Assign", token, sem);

            // The statement's position, not the token's. They usually share a
            // line, but an assignment spanning several lines gives its target a
            // position inside itself rather than the position of the statement,
            // and it is the statement the write-back stands for.
            //
            // Deliberately no fallback to the token when this is empty. It is
            // empty for a concatenation target, whose provenance is lost before
            // this transformation runs - and rebuilding it from a child here
            // would hide that upstream loss while fixing none of it, since
            // every other node that shape produces is unattributed too. That is
            // hif-frontend#37.
            //
            // The split declaration is deliberately not attributed: a later
            // refinement in this same file rebuilds signals as variables with
            // only name, type and value carried over (see "Refine to variable"),
            // so any code info set here is discarded before output. Also
            // hif-frontend#37.
            const Object::CodeInfo &origin = sourceAssign->getCodeInfo();

            // 1
            auto *sig = new Signal();
            // The split signal normally belongs beside the declaration whose
            // assignment is being split. A `for generate` is the exception, and
            // specifically a ForGenerate rather than a generate in general
            // (hif-frontend#23):
            //
            //  - the assignment may read the genvar, whose declaration lives in
            //    the ForGenerate. generateConeFunctions later copies that
            //    assignment into a procedure declared beside this signal, so a
            //    signal at module scope puts the copy out of the genvar's
            //    scope and standardization aborts on the unresolvable
            //    reference;
            //  - one signal outside the loop would be shared by every
            //    iteration, so all iterations would drive the same net.
            //
            // Neither applies to an IfGenerate: it has no index and is not
            // replicated, so its split signals stay where they have always
            // been. Moving them would be a behaviour change with no defect
            // behind it - and the declaration would land somewhere hif2verilog
            // renders incorrectly, which is hif-backend#78.
            auto *forGenerate = hif::getNearestParent<ForGenerate>(token);
            if (forGenerate != nullptr) {
                forGenerate->declarations.push_back(sig);
            } else {
                hif::manipulation::addDeclarationInContext(sig, decl, false);
            }
            sig->setName(tokenName);
            Type *tokenType = hif::semantics::getSemanticType(token, sem);
            messageAssert(tokenType != nullptr, "Cannot type token", token, sem);
            sig->setType(hif::copy(tokenType));
            if (decl->getValue() == nullptr) {
                sig->setValue(sem->getTypeDefaultValue(tokenType, decl));
            } else {
                Value *init = hif::copy(token);
                sig->setValue(init); // before getTerminalPrefix!
                Value *prefix = hif::getTerminalPrefix(init);
                prefix->replace(hif::copy(decl->getValue()));
                delete prefix;
            }

            // 2
            auto *ass = new Assign();
            ass->setLeftHandSide(hif::copy(token));
            ass->setRightHandSide(new Identifier(tokenName));
            ass->setCodeInfo(origin);
            BList<Object>::iterator jt(sourceAssign);
            jt.insert_after(ass);

            // 3
            for (auto *ref : refSet) {
                Value *topRef = refMatchesToken(ref, token);
                if (topRef == nullptr) {
                    continue;
                }
                trash.insert(topRef);
                auto *id = new Identifier(tokenName);
                topRef->replace(id);
            }
        }
    }

    // Final cleanup.
    trash.clear();
}

// ///////////////////////////////////////////////////////////////////
// Info struct
// ///////////////////////////////////////////////////////////////////

struct InfoStruct {
    InfoStruct();
    ~InfoStruct();
    InfoStruct(const InfoStruct &other);
    auto operator=(const InfoStruct &other) -> InfoStruct &;

    // Ref is port in instance binding
    RefSet portUsing;
    // Ref is in process sensitivity
    RefSet sensitivityUsing;
    // Ref is used as rhs of port binding
    RefSet bindUsing;
    // Ref is used in wait
    RefSet waitUsing;
    // Ref is rhs of continuous
    RefSet rhsContinuousUsing;
    // Ref is lhs of continuous
    RefSet lhsContinuousUsing;
    // Ref is lhs of non-blocking
    RefSet lhsNonBlockingUsing;
    // Ref is lhs of blocking
    RefSet lhsBlockingUsing;
    // Ref is read (in condition or RHS of procedural assigns or param of method call)
    RefSet readUsing;
    // Ref is an actual bound to an out/inout formal, so the call writes it
    RefSet lhsCallUsing;

    bool wasLhsContinuous{false};

    auto isOnlyInContinuousAssignments() const -> bool;
};

InfoStruct::InfoStruct()
    : portUsing()
    , sensitivityUsing()
    , bindUsing()
    , waitUsing()
    , rhsContinuousUsing()
    , lhsContinuousUsing()
    , lhsNonBlockingUsing()
    , lhsBlockingUsing()
    , readUsing()
    , lhsCallUsing()

{
    // ntd
}

InfoStruct::~InfoStruct()
{
    // ntd
}

InfoStruct::InfoStruct(const InfoStruct &other)
    : portUsing(other.portUsing)
    , sensitivityUsing(other.sensitivityUsing)
    , bindUsing(other.bindUsing)
    , waitUsing(other.waitUsing)
    , rhsContinuousUsing(other.rhsContinuousUsing)
    , lhsContinuousUsing(other.lhsContinuousUsing)
    , lhsNonBlockingUsing(other.lhsNonBlockingUsing)
    , lhsBlockingUsing(other.lhsBlockingUsing)
    , readUsing(other.readUsing)
    , lhsCallUsing(other.lhsCallUsing)
    , wasLhsContinuous(other.wasLhsContinuous)
{
    // ntd
}

auto InfoStruct::operator=(const InfoStruct &other) -> InfoStruct &
{
    if (this == &other) {
        return *this;
    }

    portUsing           = other.portUsing;
    sensitivityUsing    = other.sensitivityUsing;
    bindUsing           = other.bindUsing;
    waitUsing           = other.waitUsing;
    rhsContinuousUsing  = other.rhsContinuousUsing;
    lhsContinuousUsing  = other.lhsContinuousUsing;
    lhsNonBlockingUsing = other.lhsNonBlockingUsing;
    lhsBlockingUsing    = other.lhsBlockingUsing;
    readUsing           = other.readUsing;
    lhsCallUsing        = other.lhsCallUsing;
    wasLhsContinuous    = other.wasLhsContinuous;

    return *this;
}

using InfoMap = std::map<DataDeclaration *, InfoStruct>;

auto InfoStruct::isOnlyInContinuousAssignments() const -> bool
{
    if (!sensitivityUsing.empty()) {
        return false;
    }
    if (!bindUsing.empty()) {
        return false;
    }
    if (!waitUsing.empty()) {
        return false;
    }
    if (!lhsNonBlockingUsing.empty()) {
        return false;
    }
    if (!lhsBlockingUsing.empty()) {
        return false;
    }
    if (!readUsing.empty()) {
        return false;
    }
    // These used to be counted in readUsing, and this check kept the answer the
    // same when they moved out of it.
    if (!lhsCallUsing.empty()) {
        return false;
    }

    return true;
}

void calculateRequiredDeclType(
    bool &isConnectionSignal,
    bool &isSignal,
    bool &isVariable,
    const InfoStruct &infos,
    DataDeclaration *decl)
{
    //        bool isOutputPort = dynamic_cast<Port*>(decl) != nullptr
    //                && static_cast<Port*>(decl)->getDirection() == dir_out;

    isConnectionSignal = dynamic_cast<Port *>(decl) != nullptr || !infos.portUsing.empty() || !infos.bindUsing.empty();

    isSignal = isConnectionSignal || !infos.sensitivityUsing.empty() || !infos.waitUsing.empty() ||
               !infos.lhsNonBlockingUsing.empty();

    isVariable = !infos.lhsContinuousUsing.empty() || !infos.lhsBlockingUsing.empty() || infos.wasLhsContinuous;
}

/// @brief The formal a symbol's enclosing actual is bound to, when that formal
///        is written by the call.
///
/// An actual bound to an `out` or `inout` formal is a write, not a read. The
/// direction is the authoritative signal and is what this consults: on the
/// trees these frontends build only a task call can have such a formal, since
/// neither a Verilog nor a VHDL function declares one, but keying on the
/// direction rather than on the call's class says the rule rather than its
/// current consequence.
///
/// @param symb The symbol to classify.
/// @param sem The reference semantics.
/// @return The formal Parameter, or nullptr when the symbol is not written by a
///         call.
auto getWritingCallFormal(Object *symb, hif::semantics::ILanguageSemantics *sem) -> Parameter *
{
    auto *actual = hif::getNearestParent<ParameterAssign>(symb);
    if (actual == nullptr) {
        return nullptr;
    }

    Parameter *formal = hif::semantics::getDeclaration(actual, sem);
    if (formal == nullptr) {
        return nullptr;
    }

    const PortDirection direction = formal->getDirection();
    if (direction != dir_out && direction != dir_inout) {
        return nullptr;
    }

    return formal;
}

void fillInfoMap(RefMap &refMap, InfoMap &infoMap, bool removeProperty, hif::semantics::ILanguageSemantics *sem)
{
    for (auto &i : refMap) {
        auto *decl = dynamic_cast<DataDeclaration *>(i.first);

        auto *sig  = dynamic_cast<Signal *>(decl);
        Port *port = dynamic_cast<Port *>(decl);
        if (sig == nullptr && port == nullptr) {
            continue;
        }

        // For each interesting symbol check the context and push it into the
        // related list.
        for (auto *symb : i.second) {
            auto *ass  = hif::getNearestParent<Assign>(symb);
            Wait *wait = hif::getNearestParent<Wait>(symb);
            ObjectSensitivityOptions opts;
            opts.checkAll = true;
            if (dynamic_cast<PortAssign *>(symb) != nullptr) {
                infoMap[decl].portUsing.insert(symb);
            } else if (hif::objectIsInSensitivityList(symb)) {
                infoMap[decl].sensitivityUsing.insert(symb);
            } else if (hif::getNearestParent<PortAssign>(symb) != nullptr) {
                infoMap[decl].bindUsing.insert(symb);
            } else if (
                wait != nullptr &&
                (hif::objectIsInSensitivityList(symb, opts) || hif::isSubNode(symb, wait->getCondition()))) {
                infoMap[decl].waitUsing.insert(symb);
            } else if (getWritingCallFormal(symb, sem) != nullptr) {
                // A task's out/inout argument. This is a write, and used to
                // fall through to readUsing below because the classification
                // never consulted the formal's direction - so refineToVariables
                // renamed the reference to the shadow variable and inserted no
                // write-back, and the signal stopped updating (hif-frontend#31).
                infoMap[decl].lhsCallUsing.insert(symb);
            } else if (ass != nullptr) {
                bool isTarget    = hif::manipulation::isInLeftHandSide(symb);
                bool isInGlobact = dynamic_cast<GlobalAction *>(ass->getParent()) != nullptr;
                bool hasBlocking = !ass->checkProperty(NONBLOCKING_ASSIGNMENT);
                if (!isTarget && isInGlobact) {
                    infoMap[decl].rhsContinuousUsing.insert(symb);
                } else if (isTarget && isInGlobact) {
                    infoMap[decl].wasLhsContinuous = true;
                    infoMap[decl].lhsContinuousUsing.insert(symb);
                } else if (isTarget && !isInGlobact && !hasBlocking) {
                    if (removeProperty) {
                        ass->removeProperty(NONBLOCKING_ASSIGNMENT);
                    }
                    infoMap[decl].lhsNonBlockingUsing.insert(symb);
                } else if (isTarget && !isInGlobact && hasBlocking) {
                    infoMap[decl].lhsBlockingUsing.insert(symb);
                } else {
                    // rhs of assign
                    infoMap[decl].readUsing.insert(symb);
                }
            } else {
                // pcall, condition of if, etc.
                infoMap[decl].readUsing.insert(symb);
            }
        }
    }
}

// ///////////////////////////////////////////////////////////////////
// _fixLogicCones()
// ///////////////////////////////////////////////////////////////////
using LogicGraph     = hif::analysis::Types<DataDeclaration, DataDeclaration>::Graph;
using LogicSet       = hif::analysis::Types<DataDeclaration, DataDeclaration>::Set;
using LogicList      = hif::analysis::Types<DataDeclaration, DataDeclaration>::List;
using SymbList       = hif::semantics::SymbolList;
using ConesMap       = std::map<DataDeclaration *, Procedure *>;
using SensitivityMap = hif::analysis::Types<DataDeclaration, DataDeclaration>::Map;

void generateLogicConesGraph(InfoMap &infoMap, LogicGraph &logicGraph, hif::semantics::ILanguageSemantics *sem)
{
    // For all interesting decls (i.e. ports and signals)
    for (auto i = infoMap.begin(); i != infoMap.end(); ++i) {
        DataDeclaration *decl       = i->first;
        InfoStruct &info            = infoMap[decl];
        bool hasOnlyConstantDrivers = true;

        // The graph is related to target of continuous assigns,
        // between the target and its source symbols
        for (auto *symb : info.lhsContinuousUsing) {
            auto *ass = hif::getNearestParent<Assign>(symb);
            messageAssert(ass != nullptr, "Unexpected scope", symb, sem);

            SymbList symbolList;
            hif::semantics::collectSymbols(symbolList, ass, sem);
            for (auto *innerSymb : symbolList) {
                auto *innerId = dynamic_cast<Identifier *>(innerSymb);
                if (innerId == nullptr) {
                    hasOnlyConstantDrivers = false;
                    continue;
                }

                DataDeclaration *innerDecl = hif::semantics::getDeclaration(innerId, sem);
                messageAssert(innerDecl != nullptr, "Declaration not found", innerId, sem);

                auto *sig  = dynamic_cast<Signal *>(innerDecl);
                Port *port = dynamic_cast<Port *>(innerDecl);
                if (sig == nullptr && port == nullptr) {
                    hasOnlyConstantDrivers = false;
                    continue;
                }

                // Skipping refs to signals which are only read and never written
                // They will be moved to vars
                if (sig != nullptr) {
                    InfoStruct &infos       = infoMap[sig];
                    /*
                    if (infos.lhsBlockingUsing.empty()
                        && infos.lhsContinuousUsing.empty()
                        && infos.lhsNonBlockingUsing.empty()
                        && infos.bindUsing.empty()
                        && infos.portUsing.empty()
                        )
                        */
                    bool isConnectionSignal = false;
                    bool isSignal           = false;
                    bool isVariable         = false;
                    calculateRequiredDeclType(isConnectionSignal, isSignal, isVariable, infos, sig);
                    if (!isSignal && !isVariable) {
                        continue;
                    }
                }

                // Skipping self references: e.g. assign a = b + c (skip a)
                if (innerDecl == decl) {
                    continue;
                }

                hasOnlyConstantDrivers = false;

                // Fill the map (see typedefs in analysis.hh)
                logicGraph.first[decl].insert(innerDecl);
                logicGraph.first[innerDecl];
                logicGraph.second[innerDecl].insert(decl);
                logicGraph.second[decl];
            }
        }

        // This case:
        // assign a = 5;
        // assign b = a;
        // The signal b is only written by costant a.
        // The only problem is the initial value of b:
        // we must initialize it safely. The simplest chance is to leave the
        // assign to b, as a process which will be executed only one time, during
        // processes initialization. This supports also cases where b is written
        // partially.
        //
        // Ref design:
        // - opencCores/or1200_top
        // - custom/test_assigns
        if (hasOnlyConstantDrivers) {
            info.wasLhsContinuous = false;
            info.lhsContinuousUsing.clear();
        }
    }
}

void generateConeFunctions(
    RefMap &refMap,
    InfoMap &infoMap,
    hif::semantics::ILanguageSemantics *sem,
    LogicList &sortedGraph,
    LogicGraph &logicGraph,
    ConesMap &conesMap)
{
    hif::HifFactory f(sem);

    // Used to ensure same order of cones declarations.
    typedef std::map<std::string, Procedure *> Cones;
    Cones cones;

    for (auto *decl : sortedGraph) {
        // Skipping all shit
        auto *sig  = dynamic_cast<Signal *>(decl);
        Port *port = dynamic_cast<Port *>(decl);
        if (sig == nullptr && port == nullptr) {
            continue;
        }

        InfoStruct &infos     = infoMap[decl];
        bool onlyInCont = (infos.isOnlyInContinuousAssignments());
        if (onlyInCont) {
            continue;
        }
        // Means: signal read by continuous, but written only by processes or modules.
        // No need of cone.
        if (infos.lhsContinuousUsing.empty()) {
            continue;
        }

        // decl is used by processes (or in other places),
        // and therefore it must have its cone procedure.
        std::string pName = hif::NameTable::getInstance()->getFreshName((std::string("hif_cone_") + decl->getName()));
        Procedure *cone   = dynamic_cast<Procedure *>(f.subprogram(nullptr, pName, f.noTemplates(), f.noParameters()));

        decl->getBList()->push_back(cone);
        cones[cone->getName()] = cone;
        //hif::manipulation::addDeclarationInContext(cone, decl, false); // wrong, globact was after all decls

        StateTable *st = f.stateTable("hif_cone", f.noDeclarations(), f.noActions());
        cone->setStateTable(st);
        State *s = st->states.front();

        // Inserting the contiunuous assigns,
        // since they will be then removed from the tree.
        for (auto j = infos.lhsContinuousUsing.begin(); j != infos.lhsContinuousUsing.end(); ++j) {
            auto *ass = hif::getNearestParent<Assign>(*j);
            messageAssert(ass != nullptr, "Assign not found", *j, sem);

            s->actions.push_front(hif::copy(ass));
        }

        conesMap[decl] = cone;

        // Managing case of decl using both as target of continuous and
        // target of assing (blocking or not blocking) inside any process.
        if (!infos.lhsNonBlockingUsing.empty() || !infos.lhsBlockingUsing.empty()) {
            // adding a decl_old
            std::string dOldName = hif::NameTable::getInstance()->getFreshName((std::string("old_") + decl->getName()));
            auto *declOld        = new Variable();
            declOld->setType(hif::copy(decl->getType()));
            declOld->setValue(hif::copy(decl->getValue()));
            declOld->setName(dOldName);
            hif::manipulation::addDeclarationInContext(declOld, decl, false);

            // adding decl_tmp local to the procedure
            auto *declTmp = new Variable();
            declTmp->setType(hif::copy(decl->getType()));
            declTmp->setValue(hif::copy(decl->getValue()));
            std::string dTmpName = hif::NameTable::getInstance()->getFreshName((std::string("tmp_") + decl->getName()));
            declTmp->setName(dTmpName);
            st->declarations.push_front(declTmp);

            // Changing function body as following:
            // Original: a = b + c
            // New:
            // tmp_a = b + c;
            // if (old_a != tmp_a)
            // {
            //     old_a = tmp_a;
            //     a = tmp_a;
            // }
            auto *tmpId  = new Identifier(declTmp->getName());
            auto *oldId  = new Identifier(declOld->getName());
            auto *declId = new Identifier(decl->getName());

            for (BList<Assign>::iterator j = s->actions.toOtherBList<Assign>().begin();
                 j != s->actions.toOtherBList<Assign>().end(); ++j) {
                Assign *ass             = *j;
                std::list<Value *> tgts = hif::manipulation::collectLeftHandSideSymbols(ass);
                messageAssert(tgts.size() == 1, "Unexpected targets", ass, sem);
                Value *v = tgts.front();
                v->replace(hif::copy(tmpId));
                delete v;
            }

            f.ifStmt(
                f.noActions(), (f.ifAlt(
                                   f.expression(hif::copy(oldId), op_case_neq, hif::copy(tmpId)),
                                   (f.assignAction(oldId, hif::copy(tmpId)), f.assignAction(declId, tmpId)))));
        }

        // Update of refs and infos will be performed later.
    }

    // Fixing cone declarations order
    for (auto &i : cones) {
        Procedure *cone     = i.second;
        BList<Object> *list = cone->getBList();
        cone->replace(nullptr);
        list->push_back(cone);
    }

    for (auto i = conesMap.begin(); i != conesMap.end(); ++i) {
        DataDeclaration *decl = i->first;
        Procedure *myProc     = i->second;
        State *myState        = myProc->getStateTable()->states.front();

        for (auto *parent : sortedGraph) {
            if (logicGraph.first[decl].find(parent) == logicGraph.first[decl].end()) {
                continue;
            }

            if (conesMap.find(parent) != conesMap.end()) {
                // PCall creation moved later.
                /*
                Procedure * parentProc = conesMap[parent];

                // created procedure for parent: just call it!
                ProcedureCall * pcall = f.procedureCall(
                            parentProc->getName(),
                            nullptr,
                            f.noTemplateArguments(),
                            f.noParameterArguments()
                            );

                myState->actions.push_front(pcall);
                */
            } else {
                // push_front() of copy of all continuous assigns
                InfoStruct &parentInfos = infoMap[parent];
                for (auto k = parentInfos.lhsContinuousUsing.begin(); k != parentInfos.lhsContinuousUsing.end(); ++k) {
                    auto *ass = hif::getNearestParent<Assign>(*k);
                    messageAssert(ass != nullptr, "Assign not found", *k, sem);

                    myState->actions.push_front(hif::copy(ass));

                    // Update of refs and infos will be performed later.
                }

                // since it has been merged, update the logic graph
                // by pushing my parent-parents (i.e. grandparents)
                // as my parents! LOL!
                logicGraph.first[decl].insert(logicGraph.first[parent].begin(), logicGraph.first[parent].end());
                for (auto k = logicGraph.first[parent].begin(); k != logicGraph.first[parent].end(); ++k) {
                    logicGraph.second[*k].insert(decl);
                }
            }
        }
    }

    // Updating refs and infos
    for (auto &i : conesMap) {
        Procedure *p = i.second;

        RefMap procRefs;
        hif::semantics::getAllReferences(procRefs, sem, p);

        // updating infos
        fillInfoMap(procRefs, infoMap, true, sem);

        // updating refs
        for (auto &procRef : procRefs) {
            Declaration *decl = procRef.first;
            RefSet &refSet    = procRef.second;

            refMap[decl].insert(refSet.begin(), refSet.end());
        }
    }
}

using CallsMap = std::map<Object *, std::set<Procedure *>>;

auto getParentCone(Object *symb, ConesMap &conesMap) -> Procedure *
{
    auto *parentProc = hif::getNearestParent<Procedure>(symb);
    if (parentProc == nullptr) {
        return nullptr;
    }

    for (auto &i : conesMap) {
        if (i.second == parentProc) {
            return parentProc;
        }
    }

    return nullptr;
}

void addConesPCalls(InfoMap &infoMap, hif::semantics::ILanguageSemantics *sem, ConesMap &conesMap, CallsMap &callsMap)
{
    hif::HifFactory f(sem);

    // adding cone calls where related symbol is read or written.
    for (auto i = conesMap.begin(); i != conesMap.end(); ++i) {
        DataDeclaration *decl = i->first;
        Procedure *p          = i->second;

        InfoStruct &infos = infoMap[decl];

        RefSet tmpSet;
        tmpSet.insert(infos.readUsing.begin(), infos.readUsing.end());
        tmpSet.insert(infos.lhsBlockingUsing.begin(), infos.lhsBlockingUsing.end());
        tmpSet.insert(infos.lhsNonBlockingUsing.begin(), infos.lhsNonBlockingUsing.end());

        for (auto *symb : tmpSet) {
            auto *parentAct = hif::getNearestParent<Action>(symb);
            messageAssert(parentAct != nullptr, "cannot find parent action", symb, sem);

            // Skip symbols inside the cones itself
            if (hif::isSubNode(symb, p)) {
                continue;
            }

            // Avoid multiple calls inside same cone
            Procedure *parentCone = getParentCone(symb, conesMap);
            if (parentCone != nullptr) {
                if (callsMap[parentCone].find(p) != callsMap[parentCone].end()) {
                    continue;
                }

                callsMap[parentCone].insert(p);
            }

            // Avoid case: a = a + 1: add only one cone call.
            if (callsMap[parentAct].find(p) != callsMap[parentAct].end()) {
                continue;
            }
            callsMap[parentAct].insert(p);

            ProcedureCall *pcall =
                f.procedureCall(p->getName(), nullptr, f.noTemplateArguments(), f.noParameterArguments());

            hif::semantics::setDeclaration(pcall, p);
            if (parentCone == nullptr) {
                BList<Action>::iterator it(parentAct);
                it.insert_before(pcall);
            } else {
                // Ensuring that the pcall happens always before all the cone var refs.
                hif::semantics::GetReferencesOptions opt;
                opt.only_first = true;
                hif::semantics::ReferencesSet res;
                hif::semantics::getReferences(decl, res, sem, parentCone, opt);
                messageAssert(res.size() == 1U, "Expected just one ref", decl, sem);

                Object *firstRef = *res.begin();
                auto *firstPact  = hif::getNearestParent<Action>(firstRef);
                messageAssert(
                    firstPact != nullptr && firstPact->isInBList(), "cannot find parent action", firstRef, sem);
                BList<Action>::iterator it(firstPact);
                it.insert_before(pcall);
            }
        }
    }
}

void calculateParentNodes(DataDeclaration *decl, SensitivityMap &parentMap, SensitivityMap &sensMap)
{
    for (auto i = parentMap[decl].begin(); i != parentMap[decl].end(); ++i) {
        DataDeclaration *node = *i;
        if (sensMap[node].empty()) {
            calculateParentNodes(node, parentMap, sensMap);
        }

        if (sensMap[node].empty()) {
            // input port
            sensMap[decl].insert(node);
            continue;
        }

        // adding decl parent-parents as current decl parents.
        sensMap[decl].insert(sensMap[node].begin(), sensMap[node].end());
    }
}

void fixSensitivities(
    RefMap &refMap,
    InfoMap &infoMap,
    hif::semantics::ILanguageSemantics *sem,
    LogicGraph &logicGraph,
    ConesMap &conesMap,
    SensitivityMap &sensMap)
{
    for (auto &i : conesMap) {
        DataDeclaration *decl = i.first;

        // For all continuous,collects top-level parents in cones.
        // This can be useful for future fixes.
        calculateParentNodes(decl, logicGraph.first, sensMap);

        // Actual fix.
        // Fix only when target of continuous assignment is used in sensitivity of
        // processes or wait.
        InfoStruct &infos = infoMap[decl];
        if (infos.sensitivityUsing.empty() && infos.waitUsing.empty()) {
            continue;
        }

        // For each using adding top level parents in sensitivities and remove it
        // from the list.
        for (auto j = infos.sensitivityUsing.begin(); j != infos.sensitivityUsing.end();) {
            Object *ref            = *j;
            BList<Value> *sensList = hif::objectGetSensitivityList(ref);
            messageAssert(sensList != nullptr, "Cannot find parent sensitivity", ref, sem);
            sensList->removeSubTree(dynamic_cast<Value *>(ref));
            infos.sensitivityUsing.erase(j++);
            refMap[decl].erase(ref);
            delete ref;
            for (auto *parentNodeDecl : sensMap[decl]) {
                auto *sig  = dynamic_cast<Signal *>(parentNodeDecl);
                Port *port = dynamic_cast<Port *>(parentNodeDecl);
                if (sig == nullptr && port == nullptr) {
                    continue;
                }

                auto *sensEntry = new Identifier(parentNodeDecl->getName());
                hif::manipulation::AddUniqueObjectOptions addOpt;
                addOpt.equalsOptions.checkOnlyNames = true;
                addOpt.deleteIfNotAdded             = true;
                bool inserted                 = hif::manipulation::addUniqueObject(sensEntry, *sensList, addOpt);
                if (inserted) {
                    infoMap[parentNodeDecl].sensitivityUsing.insert(sensEntry);
                    refMap[parentNodeDecl].insert(sensEntry);
                }
            }
        }

        for (auto j = infos.waitUsing.begin(); j != infos.waitUsing.end();) {
            Object *ref = *j;
            ObjectSensitivityOptions opts;
            opts.checkAll          = true;
            BList<Value> *sensList = hif::objectGetSensitivityList(ref, opts);
            if (sensList == nullptr) {
                continue; // i.e. in wait condition
            }
            sensList->removeSubTree(dynamic_cast<Value *>(ref));
            infos.waitUsing.erase(j++);
            refMap[decl].erase(ref);
            delete ref;
            for (auto *parentNodeDecl : sensMap[decl]) {
                auto *sig  = dynamic_cast<Signal *>(parentNodeDecl);
                Port *port = dynamic_cast<Port *>(parentNodeDecl);
                if (sig == nullptr && port == nullptr) {
                    continue;
                }

                auto *sensEntry = new Identifier(parentNodeDecl->getName());
                hif::manipulation::AddUniqueObjectOptions addOpt;
                addOpt.equalsOptions.checkOnlyNames = true;
                addOpt.deleteIfNotAdded             = true;
                bool inserted                 = hif::manipulation::addUniqueObject(sensEntry, *sensList, addOpt);
                if (inserted) {
                    infoMap[parentNodeDecl].waitUsing.insert(sensEntry);
                    refMap[parentNodeDecl].insert(sensEntry);
                }
            }
        }
    }
}

void clearContinuousAssigns(RefMap &refMap, InfoMap &infoMap, hif::semantics::ILanguageSemantics *sem)
{
    for (auto i = infoMap.begin(); i != infoMap.end(); ++i) {
        //DataDeclaration * decl = i->first;
        InfoStruct &infos = i->second;

        for (auto *ref : infos.lhsContinuousUsing) {
            auto *ass = hif::getNearestParent<Assign>(ref);
            messageAssert(ass != nullptr, "Cannot find parent assign", ref, sem);

            SymbList symbolList;
            hif::semantics::collectSymbols(symbolList, ass, sem);
            for (auto *innerSymb : symbolList) {
                auto *innerId = dynamic_cast<Identifier *>(innerSymb);
                if (innerId == nullptr) {
                    continue;
                }

                DataDeclaration *innerDecl = hif::semantics::getDeclaration(innerId, sem);
                messageAssert(innerDecl != nullptr, "Declaration not found", innerId, sem);

                refMap[innerDecl].erase(innerSymb);

                auto *sig  = dynamic_cast<Signal *>(innerDecl);
                Port *port = dynamic_cast<Port *>(innerDecl);
                if (sig == nullptr && port == nullptr) {
                    continue;
                }

                infoMap[innerDecl].readUsing.erase(innerSymb);
                infoMap[innerDecl].rhsContinuousUsing.erase(innerSymb);
            }

            ass->replace(nullptr);
            delete ass;
        }

        infos.lhsContinuousUsing.clear();
    }
}

void fixLogicCones(
    RefMap &refMap,
    InfoMap &infoMap,
    ConesMap &conesMap,
    hif::semantics::ILanguageSemantics *sem,
    CallsMap &callsMap,
    SensitivityMap &sensMap)
{
    // Generate logic cones graph. It is related to target of continuous assigns,
    // between the target and its source symbols
    LogicGraph logicGraph;
    generateLogicConesGraph(infoMap, logicGraph, sem);

    // Sort the graphs w.r.t. symbols declarations.
    LogicList sortedGraph;
    hif::analysis::sortGraph<DataDeclaration, DataDeclaration>(logicGraph, sortedGraph, true);

    // create all necessary hif_cone procedures starting from sorted graph result.
    generateConeFunctions(refMap, infoMap, sem, sortedGraph, logicGraph, conesMap);

#ifdef DBG_PRINT_FIX3_STEP_FILES
    System *s = hif::getNearestParent<System>(refMap.begin()->first);
    hif::writeFile("FIX3_3_1_after_generate_cone_functions", s, true);
    hif::writeFile("FIX3_3_1_after_generate_cone_functions", s, false);
#endif

    // add all necessary calls to hif_cone procedures.
    addConesPCalls(infoMap, sem, conesMap, callsMap);

#ifdef DBG_PRINT_FIX3_STEP_FILES
    hif::writeFile("FIX3_3_2_after_add_cones_PCalls", s, true);
    hif::writeFile("FIX3_3_2_after_add_cones_PCalls", s, false);
#endif

    // fix the sensitivity list replacing symbols assigned by continuous assigns
    // with top level parents in sensitivities.
    fixSensitivities(refMap, infoMap, sem, logicGraph, conesMap, sensMap);

#ifdef DBG_PRINT_FIX3_STEP_FILES
    hif::writeFile("FIX3_3_3_after_fix_sensitivities", s, true);
    hif::writeFile("FIX3_3_3_after_fix_sensitivities", s, false);
#endif

    // removes all continuous assigns.
    clearContinuousAssigns(refMap, infoMap, sem);
}

// ///////////////////////////////////////////////////////////////////
// Refine to variables
// ///////////////////////////////////////////////////////////////////
void refineToVariables(
    RefMap &refMap,
    InfoMap &infoMap,
    ConesMap &conesMap,
    hif::semantics::ILanguageSemantics *sem,
    CallsMap &callsMap,
    SensitivityMap &sensMap)
{
    hif::HifFactory f(sem);
    hif::application_utils::WarningList bindWarnings;
    hif::application_utils::WarningList delayWarnings;

    for (auto i = infoMap.begin(); i != infoMap.end(); ++i) {
        DataDeclaration *decl = i->first;
        InfoStruct &infos     = i->second;

        bool isConnectionSignal = false;
        bool isSignal           = false;
        bool isVariable         = false;
        calculateRequiredDeclType(isConnectionSignal, isSignal, isVariable, infos, decl);

        if ((isSignal && !isVariable)) {
            // nothing to do since is already a signal.
            continue;
        }
        if ((isVariable && !isSignal) || (!isVariable && !isSignal)) {
            // Refine to variable
            auto *var = new Variable();
            var->setName(decl->getName());
            var->setType(decl->setType(nullptr));
            var->setValue(decl->setValue(nullptr));

            decl->replace(var);
            delete decl;

            for (auto *reference : refMap[decl]) {
                hif::semantics::setDeclaration(reference, var);
            }
        } else // (isVariable && isSignal) || isOutputPort
        {
            if (isConnectionSignal && !infos.bindUsing.empty()) {
                bindWarnings.push_back(decl);
            }

            // mixed case
            std::string varName = NameTable::getInstance()->getFreshName(decl->getName(), "_sig_var");

            // creating support var
            auto *var = new Variable();
            var->setName(varName);
            var->setType(hif::copy(decl->getType()));
            var->setValue(hif::copy(decl->getValue()));

            // adding after signal
            hif::manipulation::addDeclarationInContext(var, decl, false);

            messageAssert(infos.lhsContinuousUsing.empty(), "Unexpected lhs of continuous", decl, sem);

            for (auto *ref : infos.lhsBlockingUsing) {
                // 1- sig = expr  -->
                // var[5] = expr;
                // sig[5] <= var[5]; // only if is not inside cone
                // or in case of after:
                // var[5] = expr;
                // sig[5] <= expr; + warning if expr contains fcalls.

                auto *ass = hif::getNearestParent<Assign>(ref);
                messageAssert(ass != nullptr, "Parent assign not found", ref, sem);

                Assign *sigAss      = hif::copy(ass);
                bool hasDelay = (ass->getDelay() != nullptr);

                hif::objectSetName(ref, varName);
                hif::semantics::setDeclaration(ref, var);
                if (!hasDelay) {
                    delete sigAss->setRightHandSide(hif::copy(ass->getLeftHandSide()));
                } else {
                    hif::HifTypedQuery<FunctionCall> q;
                    q.check_object_method = &skipStandardFunctionCall;
                    std::list<Object *> list;
                    hif::search(list, ass->getRightHandSide(), q);

                    if (!list.empty()) {
                        delayWarnings.push_back(sigAss);
                    }
                }

                // We ceannot just skip cone related to current decl!
                // ref design: verilog/yogitech/m6502
                Procedure *parentCone = getParentCone(ref, conesMap);
                if (parentCone != nullptr) {
                    // Inside cone: skip!
                    delete sigAss;
                    continue;
                }
                BList<Assign>::iterator jt(ass);
                jt.insert_after(sigAss);
            }

            for (auto *ref : infos.lhsCallUsing) {
                // A task writes the signal through an out/inout argument:
                //   setnext(sig)  -->
                //   setnext(var);
                //   sig <= var;
                //
                // The rename alone is what the reference used to get, because
                // the classification counted it as a read. That publishes
                // nothing: the shadow is updated by the call and the signal
                // never is, so every reader freezes at whatever the last direct
                // assignment left (hif-frontend#31).
                auto *call = hif::getNearestParent<ProcedureCall>(ref);
                messageAssert(call != nullptr, "Parent call not found", ref, sem);

                auto *actual = hif::getNearestParent<ParameterAssign>(ref);
                messageAssert(actual != nullptr, "Parent actual not found", ref, sem);

                Procedure *parentCone = getParentCone(ref, conesMap);
                if (parentCone != nullptr) {
                    // Inside cone: skip, as the assignment loop above does.
                    continue;
                }

                // The target has to name the signal and the source the shadow,
                // so the two copies are taken on either side of the rename -
                // the same order the assignment loop above relies on. Copying
                // the whole actual rather than the reference keeps a partial
                // write like `t(sig[3:0])` writing back only its own bits.
                Value *target = hif::copy(actual->getValue());

                hif::objectSetName(ref, varName);
                hif::semantics::setDeclaration(ref, var);

                auto *sigAss = new Assign();
                sigAss->setCodeInfo(call->getCodeInfo());
                sigAss->setLeftHandSide(target);
                sigAss->setRightHandSide(hif::copy(actual->getValue()));

                BList<Action>::iterator jt(call);
                jt.insert_after(sigAss);
            }

            RefSet readUsing;
            readUsing.insert(infos.readUsing.begin(), infos.readUsing.end());
            readUsing.insert(infos.rhsContinuousUsing.begin(), infos.rhsContinuousUsing.end());

            for (auto *ref : readUsing) {
                // Replace sig with var

                hif::objectSetName(ref, varName);
                hif::semantics::setDeclaration(ref, var);
            }

            // Adding writing of var with sig as source in case of non-blocking
            if (!infos.lhsNonBlockingUsing.empty()) {
                BaseContents *bc = getBaseContents(decl);

                // check if decl cones method is already creaded, otherwise create it
                if (conesMap.find(decl) == conesMap.end()) {
                    std::string pName =
                        hif::NameTable::getInstance()->getFreshName((std::string("hif_cone_") + decl->getName()));
                    Procedure *cone =
                        dynamic_cast<Procedure *>(f.subprogram(nullptr, pName, f.noTemplates(), f.noParameters()));
                    bc->declarations.push_back(cone);
                    StateTable *st = f.stateTable("hif_cone", f.noDeclarations(), f.noActions());
                    cone->setStateTable(st);
                    conesMap[decl] = cone;

                    // use temporary map to add missings cones calls.
                    ConesMap tmp;
                    tmp[decl] = cone;
                    addConesPCalls(infoMap, sem, tmp, callsMap);
                }

                // added if inside cone function to assure synchronization between
                // signal and variable.

                // adding a decl_old
                std::string dOldName =
                    hif::NameTable::getInstance()->getFreshName((std::string("old_") + decl->getName()));
                auto *declOld = new Variable();
                declOld->setType(hif::copy(decl->getType()));
                declOld->setValue(hif::copy(decl->getValue()));
                declOld->setName(dOldName);
                hif::manipulation::addDeclarationInContext(declOld, decl, false);

                State *coneState = conesMap[decl]->getStateTable()->states.front();
                coneState->actions.push_front(f.ifStmt(
                    f.noActions(),
                    f.ifAlt(
                        f.expression(new Identifier(decl->getName()), op_case_neq, new Identifier(dOldName)),
                        (f.assignAction(new Identifier(dOldName), new Identifier(decl->getName())),
                         f.assignAction(new Identifier(varName), new Identifier(dOldName))))));
            }

            // Adding process to synchronize sig and var.
            if (!sensMap[decl].empty()) {
                std::string name = NameTable::getInstance()->getFreshName(
                    (std::string(var->getName()) + "_" + decl->getName() + "_sync_process"));
                StateTable *process = f.stateTable(
                    name, f.noDeclarations(),
                    f.assignAction(new Identifier(decl->getName()), new Identifier(var->getName())),
                    false //true ? no ref design found at the moment
                );

                if (conesMap.find(decl) != conesMap.end()) {
                    process->states.front()->actions.push_front(f.procedureCallAction(
                        conesMap[decl]->getName(), nullptr, f.noTemplateArguments(), f.noParameterArguments()));
                }

                for (auto *k : sensMap[decl]) {
                    process->sensitivity.push_back(f.identifier(k->getName()));
                }
                BaseContents *bc = getBaseContents(decl);
                bc->stateTables.push_back(process);
            }
        }
    }

    // raise warnings
    messageWarningList(
        true,
        "Found at least one declaration appearing in port "
        "binding and written through blocking or continuous "
        "assignment. This could lead to a non-equivalent output "
        "design description.",
        bindWarnings);

    messageWarningList(
        true,
        "Found a continuous assignment featuring a RHS "
        "containing a function call. If such function call has "
        "side effects, the generated output design description "
        "will not be equivalent.",
        delayWarnings);
}

// /////////////////////////////////////////////////////////////////////////////
// Partial flattening
// /////////////////////////////////////////////////////////////////////////////

void collectOnOutputPorts(RefMap &refMap, hif::semantics::ILanguageSemantics * /*sem*/, Views &views)
{
    // Output ports assigned by continuous assignments are replaced with explicit
    // processes.
    for (auto &i : refMap) {
        Port *p = dynamic_cast<Port *>(i.first);
        if (p == nullptr) {
            continue;
        }
        if (p->getDirection() != dir_out && p->getDirection() != dir_inout) {
            continue;
        }

        for (auto *symb : i.second) {
            auto *portAss = dynamic_cast<PortAssign *>(symb);
            if (portAss != nullptr) {
                continue;
            }
            if (!hif::manipulation::isInLeftHandSide(symb)) {
                continue;
            }
            auto *gact = hif::getNearestParent<GlobalAction>(symb);
            if (gact == nullptr) {
                continue;
            }

            View *v = hif::getNearestParent<View>(symb);
            if (v == nullptr) {
                continue;
            }
            views.insert(v);
            break;
        }
    }
}

void fillViewSets(
    DataDeclaration *decl,
    InfoStruct &infos,
    Views &views,
    ViewRefs &viewRefs,
    hif::semantics::ILanguageSemantics *sem)
{
    if (dynamic_cast<Port *>(decl) != nullptr) {
        View *v = hif::getNearestParent<View>(decl);
        if (v != nullptr) {
            views.insert(v);
        }
    }

    if (!infos.portUsing.empty()) {
        PortAssign *portAss = dynamic_cast<PortAssign *>(*infos.portUsing.begin());
        messageAssert(portAss != nullptr, "Unexpected object", *infos.portUsing.begin(), sem);
        PortAssign::DeclarationType *port = hif::semantics::getDeclaration(portAss, sem);
        messageAssert(port != nullptr, "Declaration not found", portAss, sem);
        View *v = hif::getNearestParent<View>(decl);
        if (v != nullptr) {
            views.insert(v);
        }
    }

    for (auto *i : infos.bindUsing) {
        auto *inst = hif::getNearestParent<Instance>(i);
        if (inst == nullptr) {
            continue;
        }
        auto *vr = dynamic_cast<ViewReference *>(inst->getReferencedType());
        if (vr == nullptr) {
            continue;
        }
        viewRefs.insert(vr);
    }
}

void collectOnAssigns(InfoMap &infoMap, hif::semantics::ILanguageSemantics *sem, Views &views, ViewRefs &viewRefs)
{
    for (auto &i : infoMap) {
        DataDeclaration *decl = i.first;
        InfoStruct &infos     = i.second;

        bool isConnectionSignal = false;
        bool isSignal           = false;
        bool isVariable         = false;
        calculateRequiredDeclType(isConnectionSignal, isSignal, isVariable, infos, decl);

        if ((isSignal && !isVariable)) {
            // nothing to do since is already a signal.
            continue;
        }
        if ((isVariable && !isSignal) || (!isVariable && !isSignal)) {
            // ntd
        } else // (isVariable && isSignal) || isOutputPort
        {
            if (isConnectionSignal) {
                fillViewSets(decl, infos, views, viewRefs, sem);
            }
        }
    }
}

void performPartialFlattening(System *system, Views &views, ViewRefs &viewRefs, hif::semantics::ILanguageSemantics *sem)
{
    if (views.empty() && viewRefs.empty()) {
        return;
    }

    hif::manipulation::FlattenDesignOptions fopt;
    fopt.verbose = true;

    for (auto *view : views) {
        fopt.rootDUs.insert(hif::manipulation::buildHierarchicalSymbol(view, sem));
    }

    for (auto *viewRef : viewRefs) {
        fopt.rootInstances.insert(hif::manipulation::buildHierarchicalSymbol(viewRef, sem));
    }

    hif::manipulation::flattenDesign(system, sem, fopt);
}

auto checkTopViews(Views &views, Views &topViews) -> bool
{
    for (auto *v : views) {
        if (topViews.find(v) != topViews.end()) {
            continue;
        }
        return false;
    }

    return true;
}

void performOriginalDesignChecks(RefMap &refMap, hif::semantics::ILanguageSemantics * /*sem*/)
{
    // Check non-determinism basic FSM-like case.
    // E.g.:
    // p1: @(...) sig = expr;
    // p2: @(...) out = sig;
    // This is NON-DETERMINISTIC when sig is not in p2 sensitivity.

    typedef std::set<Declaration *> Declarations;
    Declarations collectedDecls;
    typedef std::set<StateTable *> Processes;
    typedef std::map<Declaration *, Processes> ProcessesMap;
    ProcessesMap processesMap;

    // Collecting candiadate declarations
    for (auto &i : refMap) {
        Declaration *decl = i.first;
        RefSet &refSet    = i.second;
        auto *sigDecl     = dynamic_cast<Signal *>(decl);
        Port *portDecl    = dynamic_cast<Port *>(decl);
        if ((sigDecl == nullptr && portDecl == nullptr) ||
            (portDecl != nullptr && portDecl->getDirection() == hif::dir_in)) {
            continue;
        }

        for (auto *symbol : refSet) {
            // Must be in process body
            auto *state = getNearestParent<State>(symbol);
            if (state == nullptr) {
                continue;
            }
            auto *process = dynamic_cast<StateTable *>(state->getParent());
            auto *sub     = dynamic_cast<SubProgram *>(process->getParent());
            if (sub != nullptr) {
                continue; // Too hard to check!
            }
            // Skip possible initial() processes:
            if (process->getFlavour() == hif::pf_initial || process->getFlavour() == hif::pf_analog) {
                continue;
            }
            // Must be written as blocking
            bool isTarget = hif::manipulation::isInLeftHandSide(symbol);
            if (!isTarget) {
                continue;
            }
            auto *ass = hif::getNearestParent<Assign>(symbol);
            if (ass->checkProperty(NONBLOCKING_ASSIGNMENT)) {
                continue;
            }
            // Collecting
            collectedDecls.insert(decl);
            processesMap[decl].insert(process);
        }
    }

    // Checking collected candiadate declarations
    hif::application_utils::WarningSet warnings;
    for (auto *decl : collectedDecls) {
        RefSet &refSet = refMap[decl];
        for (auto *symbol : refSet) {
            // Must be in process body
            auto *state = getNearestParent<State>(symbol);
            if (state == nullptr) {
                continue;
            }
            auto *process = dynamic_cast<StateTable *>(state->getParent());
            auto *sub     = dynamic_cast<SubProgram *>(process->getParent());
            if (sub != nullptr) {
                continue; // Too hard to check!
            }
            // Skip possible initial() processes:
            if (process->getFlavour() == hif::pf_initial || process->getFlavour() == hif::pf_analog) {
                continue;
            }
            // Skip processes that both write and read: this should become
            // a local variable!
            if (processesMap[decl].find(process) != processesMap[decl].end()) {
                continue;
            }
            // Must be read
            bool isTarget = hif::manipulation::isInLeftHandSide(symbol);
            if (isTarget) {
                continue;
            }
            // Check sensitivities!
            typedef hif::HifTypedQuery<Identifier> Query;
            Query query;
            Query::Results results;
            query.onlyFirstMatch = true;
            query.name           = decl->getName();
            hif::search(results, process->sensitivity, query);
            hif::search(results, process->sensitivityPos, query);
            hif::search(results, process->sensitivityNeg, query);
            if (!results.empty()) {
                continue;
            }
            warnings.insert(decl);
            break;
        }
    }

    // Rising warnings.
    messageWarningList(
        true,
        "Found at least one signal or port written by a blocking assignment"
        " and read by a process which has not such a signal or port into its "
        "sensitivity. This could lead to non-equivalent translation.",
        warnings);
}

void partialFlattening(
    System *system,
    Views &topViews,
    RefMap &refMap,
    hif::semantics::ILanguageSemantics *sem,
    bool preserveStructure)
{
    // Flattening performed only when preserveStructure is false.
    if (preserveStructure) {
        return;
    }

    // Unreferenced signal can be safely translated as variables.
    fixUnreferenced(refMap, sem);

    Views views;
    ViewRefs viewRefs;
    // Output ports assigned by continuous assignments are collected to be flattened.
    collectOnOutputPorts(refMap, sem, views);

    // FIXME: after partially flattening the input description once, the resulting
    //        description may contain new troublesome assignments (i.e., raise new
    //        warnings). As such, partial flattening and its preliminary analysis
    //        may be required to be executed more than once. At the moment,
    //        partial flattening and its preliminary analysis are repeated
    //        in a loop. In the future, the preliminary analysis phase should be
    //        extended so that it will be necessary to carry out partial flattening
    //        only once.

    hif::semantics::GetReferencesOptions opt;
    opt.include_unreferenced = true;

    // The loop above has no natural bound: it stops when collectOnAssigns stops
    // asking for work, and nothing guarantees that ever happens.
    //
    // It does not for a design unit that was instantiated in the source and is
    // no longer instantiated after elaboration - the losing branch of an
    // `if generate`, say. collectOnAssigns still collects its view, because it
    // still drives an output port with a continuous assignment, so
    // needsFlattening stays true; but there is no instance left to flatten, so
    // performPartialFlattening changes nothing and the next round collects
    // exactly the same set. Measured on such a design: identical
    // `views={leaf, top}, viewRefs={}` from the second iteration onwards, flat
    // memory, 430 iterations a second, forever.
    //
    // Repeating a round that changed nothing cannot make progress, so stopping
    // is not a heuristic cut-off. The work left undone is flattening a design
    // unit nothing instantiates, which is what should be left undone.
    Views previousViews;
    ViewRefs previousViewRefs;
    bool hasPreviousRound = false;

    for (;;) {
        // Filling info map.
        InfoMap infoMap;
        fillInfoMap(refMap, infoMap, false, sem);

        // Collecting output ports targets of blocking/continuous assignments
        collectOnAssigns(infoMap, sem, views, viewRefs);

        // Flattening design
        bool isOnlyTop       = checkTopViews(views, topViews);
        bool needsFlattening = ((!views.empty() && !isOnlyTop) || !viewRefs.empty());
        if (!needsFlattening) {
            break;
        }
        if (hasPreviousRound && views == previousViews && viewRefs == previousViewRefs) {
            break;
        }
        previousViews    = views;
        previousViewRefs = viewRefs;
        hasPreviousRound = true;

        performPartialFlattening(system, views, viewRefs, sem);

        // Resetting infos
        refMap.clear();
        hif::semantics::resetDeclarations(system);
        hif::semantics::resetTypes(system);
        hif::semantics::typeTree(system, sem);
        hif::semantics::getAllReferences(refMap, sem, system, opt);
        views.clear();
        viewRefs.clear();
    }
}

void performStep3Refinements(hif::System *o, hif::semantics::ILanguageSemantics *sem, bool preserveStructure)
{
    // ///////////////////////////////////////////////////////////////////
    // Ensuring no concats as targets.
    // ///////////////////////////////////////////////////////////////////
    hif::manipulation::SplitAssignTargetOptions splitOpts;
    splitOpts.splitConcats     = true;
    splitOpts.createSignals    = true;
    splitOpts.splitPortassigns = true;
    hif::manipulation::splitAssignTargets(o, sem, splitOpts);

#ifdef DBG_PRINT_FIX3_STEP_FILES
    hif::writeFile("FIX3_0_after_split_concat_target", o, true);
    hif::writeFile("FIX3_0_after_split_concat_target", o, false);
#endif

    // ///////////////////////////////////////////////////////////////////
    // Updating all declarations and types
    // //////////////////////////////////////////////////////////////////
    RefMap refMap;
    hif::semantics::GetReferencesOptions opt;
    opt.include_unreferenced = true;
    hif::semantics::getAllReferences(refMap, sem, o, opt);
    hif::semantics::typeTree(o, sem);

    // ///////////////////////////////////////////////////////////////////
    // Prerefine checks
    // ///////////////////////////////////////////////////////////////////
    performOriginalDesignChecks(refMap, sem);

    // ///////////////////////////////////////////////////////////////////
    // Heuristic to avoid loops in logic cones.
    // ///////////////////////////////////////////////////////////////////
    splitLogicConesLoops(o, refMap, sem);
    refMap.clear();
    hif::semantics::getAllReferences(refMap, sem, o, opt);

#ifdef DBG_PRINT_FIX3_STEP_FILES
    hif::writeFile("FIX3_1_after_split_logic_cones", o, true);
    hif::writeFile("FIX3_1_after_split_logic_cones", o, false);
#endif

    // ///////////////////////////////////////////////////////////////////
    // Find top level
    // ///////////////////////////////////////////////////////////////////
    hif::manipulation::FindTopOptions topt;
    topt.verbose         = true;
    topt.checkAtLeastOne = true;
    Views topViews       = hif::manipulation::findTopLevelModules(o, sem, topt);

    // ///////////////////////////////////////////////////////////////////
    // Partial flattening
    // ///////////////////////////////////////////////////////////////////
    partialFlattening(o, topViews, refMap, sem, preserveStructure);

#ifdef DBG_PRINT_FIX3_STEP_FILES
    hif::writeFile("FIX3_2_after_partial_flattening", o, true);
    hif::writeFile("FIX3_2_after_partial_flattening", o, false);
#endif

    // ///////////////////////////////////////////////////////////////////
    // Expanding for generates
    // ///////////////////////////////////////////////////////////////////
    // After partial flattening, which is what specializes parameterized
    // instances, and before the logic-cone passes, which cannot represent a
    // driver that lives in a scope the cone does not.
    if (expandForGenerates(o, sem, preserveStructure)) {
        refMap.clear();
        hif::semantics::getAllReferences(refMap, sem, o, opt);
        hif::semantics::typeTree(o, sem);
    }

    // ///////////////////////////////////////////////////////////////////
    // Pre refinements
    // ///////////////////////////////////////////////////////////////////
    prerefineFixes(topViews, refMap, sem);

#ifdef DBG_PRINT_FIX3_STEP_FILES
    hif::writeFile("FIX3_3_after_prerefine_fixes", o, true);
    hif::writeFile("FIX3_3_after_prerefine_fixes", o, false);
#endif

    // ///////////////////////////////////////////////////////////////////
    // Filling info map
    // ///////////////////////////////////////////////////////////////////
    InfoMap infoMap;
    fillInfoMap(refMap, infoMap, true, sem);

    // ///////////////////////////////////////////////////////////////////
    // Fix logic cones
    // ///////////////////////////////////////////////////////////////////
    ConesMap conesMap;
    CallsMap callsMap;
    SensitivityMap sensMap;
    fixLogicCones(refMap, infoMap, conesMap, sem, callsMap, sensMap);

#ifdef DBG_PRINT_FIX3_STEP_FILES
    hif::writeFile("FIX3_4_after_fix_logic_cones", o, true);
    hif::writeFile("FIX3_4_after_fix_logic_cones", o, false);
#endif

    // ///////////////////////////////////////////////////////////////////
    // Refine in variables
    // ///////////////////////////////////////////////////////////////////
    refineToVariables(refMap, infoMap, conesMap, sem, callsMap, sensMap);
}

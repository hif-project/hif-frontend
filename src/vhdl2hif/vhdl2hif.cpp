/// @file vhdl2hif.cpp
/// @brief
/// Copyright (c) 2024-2025, Electronic Systems Design (ESD) Group,
/// Univeristy of Verona.
/// This file is distributed under the BSD 2-Clause License.
/// See LICENSE.md for details.

/////////////////////////////////////////
// Library includes
/////////////////////////////////////////

#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

/////////////////////////////////////////
// HIF library includes
/////////////////////////////////////////
#include <hif/hif.hpp>

/////////////////////////////////////////
// Tool includes
/////////////////////////////////////////

#include "vhdl2hif/vhdl2hifParseLine.hpp"
#include "vhdl2hif/vhdl_post_parsing_methods.hpp"

/////////////////////////////////////////
// Other includes
/////////////////////////////////////////
#include "vhdl2hif/vhdl_parser.hpp"

/////////////////////////////////////////
// Namespaces
/////////////////////////////////////////

using namespace hif;
using std::endl;
using std::string;

/////////////////////////////////////////
// Declaration of parser and lexer related members.
/////////////////////////////////////////

/////////////////////////////////////////
// Debug defines
/////////////////////////////////////////
//#define VHDL2HIF_PRINT_DEBUG_FILES

/////////////////////////////////////////
// Useful global variables
/////////////////////////////////////////
extern std::ostream *msgStream;
extern std::ostream *errorStream;
std::ostream *msgStream   = (&std::cout);
std::ostream *errorStream = (&std::cerr);
#if (defined _MSC_VER)
std::ostream *debugStream = new std::ofstream("nul");
#else
// To improve portability for unixes & gcc & co:
extern std::ostream *debugStream;
std::ostream *debugStream = new std::ofstream("/dev/null");
//std::ostream* debugStream = ( &std::cerr );
#endif

namespace
{

hif::application_utils::StepFileManager _stepFileManager;

} // namespace

/////////////////////////////////////////
// Utility functions prototypes
/////////////////////////////////////////

/// @brief Perform some refine of Hif description before the standardization.
/// The refinements must maintain correctness with the VHDL semantics.
void postParsingRefinements(System *systOb, bool useInt32);

/// @brief Perform some essential refines of Hif description for the printing
/// of HIF tree with printOnly option.
void performPrintOnlyRefinements(System *systOb);

/////////////////////////////////////////
// vhdl2hif main function
/////////////////////////////////////////

auto main(int argc, char *argv[]) -> int
{
    hif::application_utils::initializeLogHeader("VHDL2HIF", "");

#ifdef VHDL2HIF_PRINT_DEBUG_FILES
    _stepFileManager.setPrint(true);
#endif

    // Get command line options and arguments
    vhdl2hifParseLine cLine(argc, argv);

    std::string outputFile;
    vhdl2hifParseLine::Files inputFiles;
    std::ostringstream os;

    // Detect if verbose mode is active
    hif::application_utils::setVerboseLog(cLine.isVerbose());
    if (cLine.isWriteParsing()) {
        delete debugStream;
        debugStream = errorStream;
    }

    // Retrieve input files list
    inputFiles = cLine.getFiles();

    // Retrieve output file
    outputFile = cLine.getOutputFile();

    hif::application_utils::FileStructure fs(outputFile);
    if (!fs.isAbsolute()) {
        outputFile = fs.getAbsolutePath();
    }

    bool pslMixed = false;

    // PARSING SECTION
    for (auto &inputFile : inputFiles) {
        VhdlParser parser(inputFile);

        if (!parser.parse(cLine.isParseOnly())) {
            string msg("Cannot parse file '");
            msg = msg.append(inputFile);
            msg = msg.append("'");
            messageError(msg, nullptr, nullptr);
        }

        pslMixed |= parser.isPslMixed();
    }

    auto *vhdlLanguage = hif::semantics::VHDLSemantics::getInstance();
    vhdlLanguage->setStrictTypeChecks(false);
    vhdlLanguage->setUsePsl(pslMixed);
    auto *hifLanguage = hif::semantics::HIFSemantics::getInstance();

    // Match DesignUnits/Packages definitions with declarations collected during
    // the parsing stage. Than, populate the System object.
    System *systOb = VhdlParser::buildSystemObject();

    // Add psl standard library (if needed)
    if (pslMixed) {
        messageInfo("Found some PSL properties in the input files.");
        systOb->libraryDefs.push_back(vhdlLanguage->getStandardLibrary("psl_standard"));
    }

    if (cLine.isParseOnly()) {
        // Print translation warnings
        printUniqueWarnings("During translation, one or more warnings have been raised:");

        hif::writeFile(outputFile, systOb, true);

        if (debugStream != errorStream) {
            delete debugStream;
        }

        hif::application_utils::restoreLogHeader();
        hif::manipulation::flushInstanceCache();
        hif::semantics::flushTypeCacheEntries();
        delete systOb;
        return 0;
    }

    // PrintOnly option management
    if (cLine.isPrintOnly()) {
        performPrintOnlyRefinements(systOb);

        messageInfo("Performing HIF printing");
        hif::writeFile(outputFile, systOb, true);

        messageInfo("SUCCESS: Operation Complete");
        return 0;
    }

    _stepFileManager.printStep(systOb, "parsing_result");

    postParsingRefinements(systOb, cLine.useInt32());

    // Standardize description.
    messageInfo("Performing standardization");
    _stepFileManager.startStep("STD");
    standardizeDescription(systOb, vhdlLanguage, hifLanguage, &_stepFileManager);
    _stepFileManager.endStep(systOb);

    // Bind open port assigns.
    messageInfo("Binding open ports (if any)");
    hif::manipulation::bindOpenPortAssigns(*systOb);
    _stepFileManager.printStep(systOb, "bindOpenPortAssigns");

    // Print translation warnings
    printUniqueWarnings("During translation, one or more warnings have been raised:");

    // Finally, check description
    messageInfo("Performing HIF description sanity checks");
    hif::semantics::CheckOptions opt;
#ifndef NDEBUG
    opt.checkFlushingCaches = true;
    opt.checkSimplifiedTree = true;
#endif
    int ret = hif::semantics::checkHif(systOb, hifLanguage, opt);

    if (ret == 0) {
        hif::writeFile(outputFile, systOb, true);

        messageInfo("HIF description written in: " + outputFile);
        messageInfo("HIF translation has been completed.");
    } else {
#ifndef NDEBUG
        hif::writeFile(outputFile, systOb, true);
        hif::writeFile(outputFile, systOb, false);

        messageInfo("HIF description written in: " + outputFile);
#endif

        messageInfo("HIF translation has not been completed.");
    }

    if (debugStream != errorStream) {
        delete debugStream;
    }

    hif::application_utils::restoreLogHeader();
    hif::manipulation::flushInstanceCache();
    hif::semantics::flushTypeCacheEntries();
    delete systOb;
    return ret;
}

/////////////////////////////////////////
// Utility functions implementations
/////////////////////////////////////////

void postParsingRefinements(System *systOb, bool useInt32)
{
    hif::semantics::VHDLSemantics *vhdlSemantics = hif::semantics::VHDLSemantics::getInstance();

    // Fix ranges and libraries
    messageInfo("Performing post-parsing refinements on ranges");
    performRangeRefinements(systOb, useInt32, vhdlSemantics);
    _stepFileManager.printStep(systOb, "performRangeRefinements");

    // Add vhdl standard package
    vhdlSemantics->addStandardPackages(systOb);
    _stepFileManager.printStep(systOb, "standardPackagesRefines");

    // First Post parsing visitor
    messageInfo("Performing post-parsing refinements - step 1");
    performStep1Refinements(systOb, vhdlSemantics);
    _stepFileManager.printStep(systOb, "performStep1Refinements");

    // Reset bad declarations and types set in parsing and update all declarations.
    hif::manipulation::flushInstanceCache();
    hif::semantics::flushTypeCacheEntries();
    hif::semantics::resetTypes(systOb);
    hif::semantics::UpdateDeclarationOptions dopt;
    dopt.forceRefresh    = true;
    dopt.looseTypeChecks = true;
    updateDeclarations(systOb, vhdlSemantics, dopt);

    // Second Post parsing visitor
    messageInfo("Performing post-parsing refinements - step 2");
    performStep2Refinements(systOb, vhdlSemantics);
    _stepFileManager.printStep(systOb, "performStep2Refinements");
}

void performPrintOnlyRefinements(System *systOb)
{
    hif::semantics::ILanguageSemantics *vhdlSemantics = hif::semantics::VHDLSemantics::getInstance();

    // Add vhdl standard package
    vhdlSemantics->addStandardPackages(systOb);

    // Post parsing visitor step 1-> make HIF tree typable
    messageInfo("Performing post-parsing refinements - step 1");
    performStep1Refinements(systOb, vhdlSemantics);

    // At the moment this seems to be enough!
}

/// @file verilog2hif.cpp
/// @brief
/// Copyright (c) 2024-2025, Electronic Systems Design (ESD) Group,
/// Univeristy of Verona.
/// This file is distributed under the BSD 2-Clause License.
/// See LICENSE.md for details.

/////////////////////////////////////////
// Library includes
/////////////////////////////////////////
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <vector>

/////////////////////////////////////////
// HIF library includes
/////////////////////////////////////////
#include <hif/hif.hpp>

/////////////////////////////////////////
// Other includes
/////////////////////////////////////////
#include "verilog2hif/mark_ams_language.hpp"
#include "verilog2hif/verilog_parser.hpp"

/////////////////////////////////////////
// Tool includes
/////////////////////////////////////////
#include "verilog2hif/parse_line.hpp"
#include "verilog2hif/post_parsing_methods.hpp"
#include "verilog2hif/support.hpp"

/////////////////////////////////////////
// Namespaces
/////////////////////////////////////////
using namespace hif;

/////////////////////////////////////////
// Debug defines
/////////////////////////////////////////
//#define VERILOG2HIF_PRINT_DEBUG_FILES

/////////////////////////////////////////
// Useful global variables
/////////////////////////////////////////
extern std::string filename;
std::string filename;
std::ostream *msgStream   = (&std::cout);
std::ostream *errorStream = (&std::cerr);
#if (defined _MSC_VER)
std::ostream *debugStream = new std::ofstream("nul");
#else
// To improve portability for unixes & gcc & co:
std::ostream *debugStream = new std::ofstream("/dev/null");
#endif

// Used by lexer
extern Verilog2hifParseLine *parse_line_ptr;
Verilog2hifParseLine *parse_line_ptr = nullptr;

/////////////////////////////////////////
// Utility functions prototypes
/////////////////////////////////////////
void init_buffer(const char *);

/// @brief Perform some refine of Hif description before the standardization.
/// The refinements must maintain correctness with the Verilog semantics.
void postParsingRefinements(
    System *systOb,
    hif::semantics::VerilogSemantics *sem,
    bool needVAMSStandard,
    Verilog2hifParseLine &parse_line);

namespace
{

hif::application_utils::StepFileManager _stepFileManager;

} // namespace

/////////////////////////////////////////
// verilog2hif main function
/////////////////////////////////////////
auto main(int argc, char *argv[]) -> int
{
    hif::application_utils::initializeLogHeader("VERILOG2HIF", "");

#ifdef VERILOG2HIF_PRINT_DEBUG_FILES
    _stepFileManager.setPrint(true);
#endif

    // Get command line options and arguments
    Verilog2hifParseLine cLine(argc, argv);
    parse_line_ptr = &cLine;

    std::string outputFile;
    std::vector<std::string> inputFiles;

    // Detect if verbose mode is active
    hif::application_utils::setVerboseLog(cLine.isVerbose());
    if (cLine.isWriteParsing()) {
        delete debugStream;
        debugStream = errorStream;
    }

    // Retrieve output file
    outputFile = cLine.getOutputFile();

    // Retrieve input files list (Verilog)
    inputFiles = cLine.getFiles();
    for (auto &inputFile : inputFiles) {
        VerilogParser parser(inputFile, cLine);
        if (!parser.parse(cLine.isParseOnly())) {
            std::string msg("Cannot parse file '");
            msg = msg.append(inputFile);
            msg = msg.append("'");
            messageError(msg, nullptr, nullptr);
        }
    }

    // Retrieve input files list (VerilogA)
    inputFiles = cLine.getAmsFiles();
    VerilogParser::setVerilogAms(true);
    bool needVAMSStandard = false;
    for (auto &inputFile : inputFiles) {
        VerilogParser parser(inputFile, cLine);

        if (!parser.parse(cLine.isParseOnly())) {
            std::string msg("Cannot parse file '");
            msg = msg.append(inputFile);
            msg = msg.append("'");
            messageError(msg, nullptr, nullptr);
        }

        needVAMSStandard = true;
    }

    System *systOb = VerilogParser::buildSystemObject();

    if (cLine.isParseOnly() || cLine.isPrintOnly()) {
        hif::writeFile(outputFile, systOb, true);

        // Print translation warnings
        printUniqueWarnings("During translation, one or more warnings have been raised:");

        hif::application_utils::restoreLogHeader();
        hif::manipulation::flushInstanceCache();
        hif::semantics::flushTypeCacheEntries();
        delete systOb;
        if (debugStream != errorStream) {
            delete debugStream;
        }

        return 0;
    }

    _stepFileManager.printStep(systOb, "parsing_result");

    auto *verilogLanguage = hif::semantics::VerilogSemantics::getInstance();
    auto *hifLanguage     = hif::semantics::HIFSemantics::getInstance();

    // fix description
    postParsingRefinements(systOb, verilogLanguage, needVAMSStandard, cLine);

    // Standardize description.
    messageInfo("Performing standardization");
    _stepFileManager.startStep("STD");
    standardizeDescription(systOb, verilogLanguage, hifLanguage, &_stepFileManager);
    _stepFileManager.endStep(systOb);

    // Marking AMS.
    messageInfo("Refining possible AMS units");
    if (needVAMSStandard) {
        markAmsLanguage(systOb, hifLanguage);
    }
    _stepFileManager.printStep(systOb, "markAmsLanguage");

    // Finally, check description
    messageInfo("Performing HIF description sanity checks");
    hif::semantics::CheckOptions opt;
#ifndef NDEBUG
    opt.checkFlushingCaches = true;
    opt.checkSimplifiedTree = true;
#endif

    // Print translation warnings
    printUniqueWarnings("During translation, one or more Warnings have been raised:");

    // Check description
    messageInfo("Performing final HIF sanity checks");
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

void postParsingRefinements(
    System *systOb,
    hif::semantics::VerilogSemantics *sem,
    bool needVAMSStandard,
    Verilog2hifParseLine &cLine)
{
    // Add Verilog standard package
    if (needVAMSStandard) {
        systOb->libraryDefs.push_back(sem->getStandardLibrary("vams_standard"));

        // The following code is a workaround until the inclusion issue will be fixed in lexer
        systOb->libraryDefs.push_back(sem->getStandardLibrary("vams_constants"));
        systOb->libraryDefs.push_back(sem->getStandardLibrary("vams_disciplines"));
        systOb->libraryDefs.push_back(sem->getStandardLibrary("vams_driver_access"));
        hif::HifFactory f(sem);
        systOb->libraries.push_back(f.library("vams_standard", nullptr, "", false, true));
        systOb->libraries.push_back(f.library("vams_constants", nullptr, "", false, true));
        systOb->libraries.push_back(f.library("vams_disciplines", nullptr, "", false, true));
        systOb->libraries.push_back(f.library("vams_driver_access", nullptr, "", false, true));
    }
    sem->addStandardPackages(systOb);
    _stepFileManager.printStep(systOb, "standardPackagesRefines");

    messageInfo("Performing post-parsing refinements - step 1");
    performStep1Refinements(systOb, sem);
    _stepFileManager.printStep(systOb, "performStep1Refinements");

    messageInfo("Performing post-parsing refinements - step 2");
    performStep2Refinements(systOb, sem);
    _stepFileManager.printStep(systOb, "performStep2Refinements");

    messageInfo("Renaming conflicting declarations");
    hif::manipulation::renameConflictingDeclarations(systOb, sem, nullptr, "inst");
    _stepFileManager.printStep(systOb, "renameConflictingDeclarations");

    // Bind open port assigns.
    messageInfo("Binding open ports (if any)");
    hif::manipulation::bindOpenPortAssigns(*systOb, sem);
    _stepFileManager.printStep(systOb, "bindOpenPortAssigns");

    messageInfo("Performing post-parsing refinements - step 3");
    performStep3Refinements(systOb, sem, cLine.getStructure());
    _stepFileManager.printStep(systOb, "performStep3Refinements");
}

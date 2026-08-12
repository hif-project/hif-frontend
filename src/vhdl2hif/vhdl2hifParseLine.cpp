/// @file vhdl2hifParseLine.cpp
/// @brief
/// Copyright (c) 2024-2025, Electronic Systems Design (ESD) Group,
/// Univeristy of Verona.
/// This file is distributed under the BSD 2-Clause License.
/// See LICENSE.md for details.

#include "vhdl2hif/vhdl2hifParseLine.hpp"

vhdl2hifParseLine::vhdl2hifParseLine(int argc, char *argv[])
    : CommandLineParser()
{
    addToolInfos(
        // Tool name.
        "vhdl2hif",
        // Copyright.
        "Copyright (c) 2024-2025, Electronic Systems Design (ESD) Group, Univeristy of Verona."
        "This file is distributed under the BSD 2-Clause License.",
        // description
        "Generates HIF from a VHDL description.",
        // synopsys
        "vhdl2hif [OPTIONS] <VHDL FILES>",
        // notes
        "- The default output file name is out.hif.xml.\n"
        "- The output file specified via '-o' can have no extension.\n"
        "\n"
        "Email: enrico.fraccaroli@univr.it    Site: Coming Soon");

    addHelp();
    addVersion();
    addVerbose();
    addOutputFile();
    addPrintOnly();
    addParseOnly();
    addWriteParsing();

#ifdef NDEBUG
    bool useSynthesisIntAvaiable = false;
#else
    bool useSynthesisIntAvaiable = true;
#endif

    addOption(
        'i', "integers", false, useSynthesisIntAvaiable,
        "(Experimental) Translate integers using span computed from "
        "specified range, instead of assuming span of 32 bits.");

    parse(argc, argv);

    _validateArguments();
}

vhdl2hifParseLine::~vhdl2hifParseLine()
{
    // ntd
}

void vhdl2hifParseLine::_validateArguments()
{
    if (!_options['h'].value.empty()) {
        printHelp();
    }
    if (!_options['v'].value.empty()) {
        printVersion();
    }

    if (_files.empty()) {
        messageError(
            "VHDL input file missing.\n"
            "Try 'vhdl2hif --help' for more information",
            nullptr, nullptr);
    }

    // Validate input file list
    for (auto &_file : _files) {
        std::string cleanFile = _cleanFileName(_file);
        if (!_checkVhdlFile(cleanFile) && !_checkPslFile(cleanFile)) {
            messageError("Unrecognized input file:" + _file, nullptr, nullptr);
        }
    }

    // Establish output file name
    std::string out = _options['o'].value;
    if (out.empty()) {
        out = "out.hif.xml";
    } else {
        std::string::size_type ix = out.find(".hif.xml");
        if (ix == std::string::npos) {
            out += ".hif.xml";
        }
    }
    _options['o'].value = out;
}

auto vhdl2hifParseLine::_checkVhdlFile(const std::string &inputFile) -> bool
{
    return inputFile.find(".vhd") != std::string::npos || inputFile.find(".vhdl") != std::string::npos;
}

auto vhdl2hifParseLine::_checkPslFile(const std::string &inputFile) -> bool
{
    return inputFile.find(".psl") != std::string::npos;
}

auto vhdl2hifParseLine::_cleanFileName(const std::string &inputString) -> std::string
{
    size_t found = inputString.find_last_of('/');

    return (found != std::string::npos) ? inputString.substr(found + 1) : inputString;
}

auto vhdl2hifParseLine::useInt32() -> bool { return getOption('i').empty(); }

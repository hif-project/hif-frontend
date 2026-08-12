/// @file verilog2hif_parse_line.cpp
/// @brief
/// Copyright (c) 2024-2025, Electronic Systems Design (ESD) Group,
/// Univeristy of Verona.
/// This file is distributed under the BSD 2-Clause License.
/// See LICENSE.md for details.

#include "verilog2hif/parse_line.hpp"

static inline auto checkExtension(const std::string &file_name, const std::string &extension) -> bool
{
    std::string::size_type size  = 0;
    std::string::size_type index = 0;
    size                         = file_name.size();
    index                        = file_name.find(extension, 0);

    return (index != std::string::npos) && (size - index == extension.size());
}

Verilog2hifParseLine::Verilog2hifParseLine(int argc, char *argv[])
    : CommandLineParser()
    , _amsFiles()
{
    addToolInfos(
        // Tool name.
        "verilog2hif",
        // Copyright.
        "Copyright (c) 2024-2025, Electronic Systems Design (ESD) Group, Univeristy of Verona."
        "This file is distributed under the BSD 2-Clause License.",
        // description
        "Generates HIF from a Verilog/Verilog-AMS description.",
        // synopsys
        "verilog2hif [OPTIONS] <VERILOG FILES>",
        // notes
        "- The default output file name is out.hif.\n"
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
    addOption('t', "ternary", false, true, "Consider ternary operators without 'X' and 'Z' cases.");
    addOption(
        's', "structure", false, true,
        "Preserve design structure even when this could lead to "
        "non-equivalent translation.");

    parse(argc, argv);

    _validateArguments();
}

Verilog2hifParseLine::~Verilog2hifParseLine()
{
    // ntd
}

auto Verilog2hifParseLine::getTernary() const -> bool { return isOptionFlagSet('t'); }

auto Verilog2hifParseLine::getStructure() const -> bool { return isOptionFlagSet('s'); }

void Verilog2hifParseLine::_validateArguments()
{
    if (!_options['h'].value.empty()) {
        printHelp();
    }
    if (!_options['v'].value.empty()) {
        printVersion();
    }

    if (_files.empty()) {
        messageError(
            "Verilog input file missing.\n"
            "Try 'verilog2hif --help' for more information",
            nullptr, nullptr);
    }

    // Validate input file list
    for (auto i = _files.begin(); i != _files.end();) {
        std::string cleanFile = _cleanFileName(*i);
        if (_checkVerilogFile(cleanFile)) {
            // OK
            ++i;
        } else if (_checkVerilogAmsFile(cleanFile)) {
            _amsFiles.push_back(*i);
            i = _files.erase(i);
        } else {
            messageError("Unrecognized file format:" + cleanFile, nullptr, nullptr);
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

auto Verilog2hifParseLine::_checkVerilogFile(const std::string &fileName) -> bool
{
    return checkExtension(fileName, ".v");
}

auto Verilog2hifParseLine::_checkVerilogAmsFile(const std::string &fileName) -> bool
{
    return checkExtension(fileName, ".va") || checkExtension(fileName, ".vams");
}

auto Verilog2hifParseLine::_cleanFileName(const std::string &fileName) -> std::string
{
    std::string cleanFile;
    size_t found = fileName.find_last_of('/');
    if (found != std::string::npos) {
        cleanFile = fileName.substr(found + 1);
    } else {
        cleanFile = fileName;
    }

    return cleanFile;
}

auto Verilog2hifParseLine::getAmsFiles() -> std::vector<std::string> { return _amsFiles; }

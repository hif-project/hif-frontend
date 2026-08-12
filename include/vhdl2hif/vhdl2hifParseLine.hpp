/// @file vhdl2hifParseLine.hpp
/// @brief Header file for the vhdl2hifParseLine class.
/// Copyright (c) 2024-2025, Electronic Systems Design (ESD) Group,
/// Univeristy of Verona.
/// This file is distributed under the BSD 2-Clause License.
/// See LICENSE.md for details.

#pragma once

#include <hif/hif.hpp>

class vhdl2hifParseLine : public hif::application_utils::CommandLineParser
{
public:
    vhdl2hifParseLine(int argc, char *argv[]);

    ~vhdl2hifParseLine() override;

    auto useInt32() -> bool;

private:
    /// @brief Validates and configures the arguments.
    void _validateArguments();

    /// @brief Function to check if the source file parameter
    /// is a valid name for VHDL source.
    ///
    /// @param input_file string name of source the file.
    ///
    /// @return <tt>true</tt> if the input_file is a correct VHDL source file, <tt><false/tt> otherwise.
    //  The correct shape of the VHDL source file is : file_name.vhd or file_name.vhdl.
    ///
    static auto _checkVhdlFile(const std::string &input_file) -> bool;

    /// @brief Function to check if the source file parameter
    /// is a valid name for PSL source.
    ///
    /// @param input_file string name of source the file.
    ///
    /// @return <tt>true</tt> if the input_file is a correct PSL source file, <tt><false/tt> otherwise.
    //  The correct shape of the PSL source file is : file_name.psl.
    ///
    static auto _checkPslFile(const std::string &input_file) -> bool;

    /// @brief Function to extract the name of the VHDL source file.
    ///
    /// @param input_string string name of the file.
    ///
    /// @return string containing only the name of the file. If <tt>input_string</tt> is a path
    /// (e.g.: ../dir1/dir2/foo.vhd), the function returns the file name (foo.vhd).
    ///
    static auto _cleanFileName(const std::string &input_string) -> std::string;
};

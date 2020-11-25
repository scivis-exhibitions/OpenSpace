/*****************************************************************************************
 *                                                                                       *
 * OpenSpace                                                                             *
 *                                                                                       *
 * Copyright (c) 2014-2020                                                               *
 *                                                                                       *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this  *
 * software and associated documentation files (the "Software"), to deal in the Software *
 * without restriction, including without limitation the rights to use, copy, modify,    *
 * merge, publish, distribute, sublicense, and/or sell copies of the Software, and to    *
 * permit persons to whom the Software is furnished to do so, subject to the following   *
 * conditions:                                                                           *
 *                                                                                       *
 * The above copyright notice and this permission notice shall be included in all copies *
 * or substantial portions of the Software.                                              *
 *                                                                                       *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,   *
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A         *
 * PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT    *
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF  *
 * CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE  *
 * OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                                         *
 ****************************************************************************************/

#include <openspace/interaction/tasks/convertrecformattask.h>

#include <openspace/interaction/sessionrecording.h>
#include <openspace/interaction/sessionrecording_data.h>
#include <openspace/documentation/verifier.h>
#include <ghoul/filesystem/file.h>
#include <ghoul/filesystem/filesystem.h>
#include <ghoul/logging/logmanager.h>
#include <filesystem>

namespace {
    constexpr const char* _loggerCat = "ConvertRecFormatTask";

    constexpr const char* KeyInFilePath = "InputFilePath";
    constexpr const char* KeyOutFilePath = "OutputFilePath";
    constexpr const char* KeyOutputFormat = "OutputFormat";
} // namespace

namespace openspace::interaction {

ConvertRecFormatTask::ConvertRecFormatTask(const ghoul::Dictionary& dictionary) {
    openspace::documentation::testSpecificationAndThrow(
        documentation(),
        dictionary,
        "ConvertRecFormatTask"
    );

    _inFilePath = absPath(dictionary.value<std::string>(KeyInFilePath));
    _outFilePath = absPath(dictionary.value<std::string>(KeyOutFilePath));

    std::string format = dictionary.value<std::string>(KeyOutputFormat);
    if (format == "ASCII") {
        _fileFormatType = sessionrecording::DataMode::Ascii;
    }
    else {
        _fileFormatType = sessionrecording::DataMode::Binary;
    }

    ghoul_assert(FileSys.fileExists(_inFilePath), "The filename must exist");
    if (!FileSys.fileExists(_inFilePath)) {
        LERROR(fmt::format("Failed to load session recording file: {}", _inFilePath));
    }
}

std::string ConvertRecFormatTask::description() {
    std::string description = "Convert session recording file '" + _inFilePath + "' ";
    if (_fileFormatType == sessionrecording::DataMode::Ascii) {
        description += "(ascii format) ";
    }
    else if (_fileFormatType == sessionrecording::DataMode::Binary) {
        description += "(binary format) ";
    }
    else {
        description += "(UNKNOWN format) ";
    }
    description += "conversion to file '" + _outFilePath + "'.";
    return description;
}

void ConvertRecFormatTask::perform(const Task::ProgressCallback&) {
    using namespace sessionrecording;
    SessionRecordingData data;
    data.read(_inFilePath);
    data.write(_outFilePath, _fileFormatType);
}

documentation::Documentation ConvertRecFormatTask::documentation() {
    using namespace documentation;
    return {
        "ConvertRecFormatTask",
        "convert_format_task",
        {
            {
                "Type",
                new StringEqualVerifier("ConvertRecFormatTask"),
                Optional::No,
                "The type of this task",
            },
            {
                KeyInFilePath,
                new StringAnnotationVerifier("A valid filename to convert"),
                Optional::No,
                "The filename to convert to the opposite format.",
            },
            {
                KeyOutFilePath,
                new StringAnnotationVerifier("A valid output filename"),
                Optional::No,
                "The filename containing the converted result.",
            },
            {
                KeyOutputFormat,
                new StringListVerifier( { "ASCII", "Binary"} ),
                Optional::No,
                "The format that the session recording should be converted to"
            }
        },
    };
}

} // namespace openspace::interaction

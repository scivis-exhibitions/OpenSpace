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

#ifndef __OPENSPACE_CORE___SESSIONRECORDING_DATA___H__
#define __OPENSPACE_CORE___SESSIONRECORDING_DATA___H__

#include <openspace/network/messagestructures.h>
#include <filesystem>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace openspace::interaction::sessionrecording {

struct ConversionError : public ghoul::RuntimeError {
    explicit ConversionError(std::string msg);
};

// How to update the version number:
//   - Add a new namespace
//   - Mark that namespace as `inline` and remove the `inline` from the previous version
//   - Mark the `writeSessionRecording` from the previous version as `= delete`
//   - Add the changes that make the version new into the new namespace
//   - Add functions:
//     * `SessionRecordingData updateVersion(versionX::SessionRecordingData)` that takes a
//       session recording from versionX and returns a session recording object from your
//       new version
//     * `bool writeSessionRecording(const SessionRecordingData&, DataMode)` that writes
//       the passed session recording to disk in the provided data mode
//   - Add a new case to the if statement in the `convertSessionRecordingFile` function

namespace version1 {
    constexpr const std::string_view Version = "00.85";

    enum class DataMode {
        Ascii = 0,
        Binary,
        Unknown
    };

    struct Timestamps {
        double timeOs;
        double timeRec;
        double timeSim;
    };

    // @VOLATILE (abock, 2020-11-12):  This should be changed by moving copies of those
    // data structures into this version.  There is no real need to have these structures
    // be reused between the parallel sessions and the session recording.  Would be nice
    // to be able to convert between the two, but there is no need to binary compatibility
    using CameraKeyframe = datamessagestructures::CameraKeyframe;
    using TimeKeyframe = datamessagestructures::TimeKeyframe;
    using ScriptMessage = datamessagestructures::ScriptMessage;
    struct CommentMessage {
        std::string comment;
    };

    struct Frame {
        Timestamps time;
        std::variant<CameraKeyframe, TimeKeyframe, ScriptMessage, CommentMessage> message;
    };
    using SessionRecordingData = std::vector<Frame>;

    DataMode characterToDataMode(unsigned char c);
    // There is no version method since this is the first version
    SessionRecordingData readSessionRecording(const std::string& filename, DataMode mode);
    void writeSessionRecording(const SessionRecordingData& data,
        const std::string& filename, DataMode mode) = delete;
} // namespace version1


inline namespace version2 {
    constexpr const std::string_view Version = "01.00";

    // This version only had changes in the way the session recording was loaded, so we
    // retain all of the data structures from the previous version
    using DataMode = version1::DataMode;
    using Timestamps = version1::Timestamps;
    using CameraKeyframe = version1::CameraKeyframe;
    using TimeKeyframe = version1::TimeKeyframe;
    using ScriptMessage = version1::ScriptMessage;
    using Frame = version1::Frame;
    using SessionRecordingData = version1::SessionRecordingData;

    SessionRecordingData updateVersion(version1::SessionRecordingData sessionRecording);
    SessionRecordingData readSessionRecording(const std::string& filename, DataMode mode);
    void writeSessionRecording(const SessionRecordingData& data,
        const std::string& filename, DataMode mode);

} // namespace version2

// The header format has to be the same for all session recording versions
struct Header {
    static constexpr const std::string_view Title = "OpenSpace_record/playback";
    static constexpr const int VersionLength = 5;

    std::string version;
    char dataMode;
};
Header readHeader(std::istream& stream);

std::filesystem::path convertSessionRecordingFile(const std::filesystem::path& path);

} // namespace openspace::interaction::sessionrecording

#endif // __OPENSPACE_CORE___SESSIONRECORDING_DATA___H__

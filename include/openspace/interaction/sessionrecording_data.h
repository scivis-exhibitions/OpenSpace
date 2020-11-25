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

#include <ghoul/misc/exception.h>
#include <ghoul/glm.h>
#include <filesystem>
#include <string_view>
#include <variant>
#include <vector>

namespace openspace::interaction::sessionrecording {

struct ConversionError : public ghoul::RuntimeError {
    explicit ConversionError(std::string msg);
};

enum class DataMode { Ascii, Binary };

// The header format has to be the same for all session recording versions
struct Header {
    static constexpr const std::string_view Title = "OpenSpace_record/playback";
    static constexpr const int VersionLength = 5;

    void read(std::istream& stream);

    std::string version;
    DataMode dataMode;
};

namespace version1 {
    constexpr const std::string_view Version = "00.85";

    struct Timestamp {
        //~Timestamp() = default;
        /*virtual*/ void read(std::istream& stream, DataMode mode);
        /*virtual*/ void write(std::ostream& stream, DataMode mode) const;

        double timeOs;
        double timeRec;
        double timeSim;
    };

    struct CameraMessage {
        //virtual ~CameraMessage() = default;
        /*virtual*/ void read(std::istream& stream, DataMode mode);
        /*virtual*/ void write(std::ostream& stream, DataMode mode) const;

        Timestamp time;
        glm::dvec3 position;
        glm::dquat rotation;
        bool followNodeRotation;
        std::string focusNode;
        float scale;
        double timestamp;
    };

    struct TimeMessage {
        //virtual ~TimeMessage() = default;
        /*virtual*/ void read(std::istream& stream, DataMode mode);
        /*virtual*/ void write(std::ostream& stream, DataMode mode) const;

        Timestamp time;
        double timeUnused; // is still in here for binary compatibility
        double dt;
        bool paused;
        bool requiresTimeJump;
        double timestamp;
    };

    struct ScriptMessage {
        //virtual ~ScriptMessage() = default;
        /*virtual*/ void read(std::istream& stream, DataMode mode);
        /*virtual*/ void write(std::ostream& stream, DataMode mode) const;

        Timestamp time;
        std::string script;
        double timestamp;
    };

    struct CommentMessage {
        //virtual ~CommentMessage() = default;
        /*virtual*/ void read(std::istream& stream, DataMode mode);
        /*virtual*/ void write(std::ostream& stream, DataMode mode) const;

        std::string comment;
    };

    struct Frame {
        //virtual ~Frame() = default;
        /*virtual*/ bool read(std::istream& stream, DataMode mode);
        /*virtual*/ void write(std::ostream& stream, DataMode mode) const;

        std::variant<CameraMessage, TimeMessage, ScriptMessage, CommentMessage> message;
    };

    struct SessionRecordingData {
        //virtual ~SessionRecordingData() = default;
        /*virtual*/ void read(const std::string& filename);
        /*virtual*/ void write(const std::string& filename, DataMode mode) const;

        Header header;
        std::vector<Frame> frames;
    };

} // namespace version1

inline namespace version2 {
    constexpr const std::string_view Version = "01.00";

    using Timestamp = version1::Timestamp;
    using CameraMessage = version1::CameraMessage;
    using TimeMessage = version1::TimeMessage;
    using CommentMessage = version1::CommentMessage;

    struct ScriptMessage : public version1::ScriptMessage {
        /*virtual*/ void read(std::istream& stream, DataMode mode);
        /*virtual*/ void write(std::ostream& stream, DataMode mode) const;
    };

    struct Frame {
        //virtual ~Frame() = default;
        /*virtual*/ bool read(std::istream& stream, DataMode mode);
        /*virtual*/ void write(std::ostream& stream, DataMode mode) const;

        std::variant<CameraMessage, TimeMessage, ScriptMessage, CommentMessage> message;
    };

    struct SessionRecordingData {
        //virtual ~SessionRecordingData() = default;
        /*virtual*/ void read(const std::string& filename);
        /*virtual*/ void write(const std::string& filename, DataMode mode) const;

        Header header;
        std::vector<Frame> frames;
    };


    std::filesystem::path convertSessionRecordingFile(const std::filesystem::path& path);

} // namespace version2

} // namespace openspace::interaction::sessionrecording

#if 0

#include <ghoul/misc/exception.h>
#include <ghoul/fmt.h>
#include <ghoul/glm.h>
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

    struct Timestamp {
        double timeOs;
        double timeRec;
        double timeSim;
    };

    struct CameraMessage {
        glm::dvec3 position;
        glm::dquat rotation;
        bool followNodeRotation;
        std::string focusNode;
        float scale;
        double timestamp;
    };
    
    struct TimeMessage {
        double time;
        double dt;
        bool paused;
        bool requiresTimeJump;
        double timestamp;
    };
    
    struct ScriptMessage {
        std::string script;
        double timestamp;
    };

    struct CommentMessage {
        std::string comment;
    };

    struct Frame {
        Timestamp time;
        std::variant<CameraMessage, TimeMessage, ScriptMessage, CommentMessage> message;
    };
    using SessionRecordingData = std::vector<Frame>;

    DataMode characterToDataMode(unsigned char c);
    SessionRecordingData readSessionRecording(const std::string& filename);
    void writeSessionRecording(const SessionRecordingData& data,
        const std::string& filename, DataMode mode) = delete;
} // namespace version1


inline namespace version2 {
    constexpr const std::string_view Version = "01.00";

    // This version only had changes in the way the session recording was loaded, so we
    // retain all of the data structures from the previous version
    using DataMode = version1::DataMode;
    using Timestamp = version1::Timestamp;
    using CameraMessage = version1::CameraMessage;
    using TimeMessage = version1::TimeMessage;
    using ScriptMessage = version1::ScriptMessage;
    using Frame = version1::Frame;
    using SessionRecordingData = version1::SessionRecordingData;

    SessionRecordingData updateVersion(version1::SessionRecordingData sessionRecording);
    SessionRecordingData readSessionRecording(const std::string& filename);
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

#endif

#endif // __OPENSPACE_CORE___SESSIONRECORDING_DATA___H__

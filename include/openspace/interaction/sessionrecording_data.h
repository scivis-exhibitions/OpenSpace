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
        /*virtual*/ void readAscii(std::istream& stream);
        /*virtual*/ void readBinary(std::istream& stream);
        /*virtual*/ void writeAscii(std::ostream& stream) const;
        /*virtual*/ void writeBinary(std::ostream& stream) const;

        double timeOs;
        double timeRec;
        double timeSim;
    };

    struct CameraMessage {
        static constexpr const std::string_view AsciiKey = "camera";
        static constexpr const char BinaryKey = 'c';

        //virtual ~CameraMessage() = default;
        /*virtual*/ void readAscii(std::istream& stream);
        /*virtual*/ void readBinary(std::istream& stream);
        /*virtual*/ void writeAscii(std::ostream& stream) const;
        /*virtual*/ void writeBinary(std::ostream& stream) const;

        Timestamp time;
        glm::dvec3 position;
        glm::dquat rotation;
        bool followNodeRotation;
        std::string focusNode;
        float scale;
        double timestamp;
    };

    struct TimeMessage {
        static constexpr const std::string_view AsciiKey = "time";
        static constexpr const char BinaryKey = 't';

        //virtual ~TimeMessage() = default;
        /*virtual*/ void readAscii(std::istream& stream);
        /*virtual*/ void readBinary(std::istream& stream);
        /*virtual*/ void writeAscii(std::ostream& stream) const;
        /*virtual*/ void writeBinary(std::ostream& stream) const;

        Timestamp time;
        double timeUnused; // is still in here for binary compatibility
        double dt;
        bool paused;
        bool requiresTimeJump;
        double timestamp;
    };

    struct ScriptMessage {
        static constexpr const std::string_view AsciiKey = "script";
        static constexpr const char BinaryKey = 's';

        //virtual ~ScriptMessage() = default;
        /*virtual*/ void readAscii(std::istream& stream);
        /*virtual*/ void readBinary(std::istream& stream);
        /*virtual*/ void writeAscii(std::ostream& stream) const;
        /*virtual*/ void writeBinary(std::ostream& stream) const;

        Timestamp time;
        std::string script;
        double timestamp;
    };

    struct CommentMessage {
        static constexpr const std::string_view AsciiKey = "#";
        static constexpr const char BinaryKey = '#';

        //virtual ~CommentMessage() = default;
        /*virtual*/ void readAscii(std::istream& stream);
        /*virtual*/ void readBinary(std::istream& stream);
        /*virtual*/ void writeAscii(std::ostream& stream) const;
        /*virtual*/ void writeBinary(std::ostream& stream) const;

        std::string comment;
    };

    template <typename... MessageTypes>
    struct GenericFrame {
        //virtual ~Frame() = default;
        /*virtual*/ bool readAscii(std::istream& stream);
        /*virtual*/ bool readBinary(std::istream& stream);
        /*virtual*/ void writeAscii(std::ostream& stream) const;
        /*virtual*/ void writeBinary(std::ostream& stream) const;

        //std::variant<CameraMessage, TimeMessage, ScriptMessage, CommentMessage> message;
        std::variant<MessageTypes...> message;
    };

    using Frame = GenericFrame<CameraMessage, TimeMessage, ScriptMessage, CommentMessage>;

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
        ScriptMessage() = default;
        ScriptMessage(const version1::ScriptMessage& msg);
        /*virtual*/ void readBinary(std::istream& stream);
        /*virtual*/ void writeBinary(std::ostream& stream) const;
    };

    struct Frame {
        Frame() = default;
        Frame(const version1::Frame& frame);
        //virtual ~Frame() = default;
        /*virtual*/ bool readAscii(std::istream& stream);
        /*virtual*/ bool readBinary(std::istream& stream);
        /*virtual*/ void writeAscii(std::ostream& stream) const;
        /*virtual*/ void writeBinary(std::ostream& stream) const;

        std::variant<CameraMessage, TimeMessage, ScriptMessage, CommentMessage> message;
    };

    struct SessionRecordingData {
        SessionRecordingData() = default;
        SessionRecordingData(const version1::SessionRecordingData& data);
        //virtual ~SessionRecordingData() = default;
        /*virtual*/ void read(const std::string& filename);
        /*virtual*/ void write(const std::string& filename, DataMode mode) const;

        Header header;
        std::vector<Frame> frames;
    };


    std::filesystem::path convertSessionRecordingFile(const std::filesystem::path& path);

} // namespace version2

} // namespace openspace::interaction::sessionrecording

#endif // __OPENSPACE_CORE___SESSIONRECORDING_DATA___H__

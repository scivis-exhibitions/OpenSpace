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
    void write(std::ostream& stream) const;

    std::string version;
    DataMode dataMode;
};

template <typename... MessageTypes>
struct GenericFrame {
    bool readAscii(std::istream& stream);
    bool readBinary(std::istream& stream);
    void writeAscii(std::ostream& stream) const;
    void writeBinary(std::ostream& stream) const;

    std::variant<MessageTypes...> message;
};

template <typename HeaderType, typename FrameType>
struct GenericSessionRecordingData {
    void read(const std::string& filename);
    void write(const std::string& filename, DataMode mode) const;

    HeaderType header;
    std::vector<FrameType> frames;
};

namespace version1 {
    constexpr const std::string_view Version = "00.85";

    struct Timestamp {
        void readAscii(std::istream& stream);
        void readBinary(std::istream& stream);
        void writeAscii(std::ostream& stream) const;
        void writeBinary(std::ostream& stream) const;

        double timeOs;
        double timeRec;
        double timeSim;
    };

    struct CameraMessage {
        static constexpr const std::string_view AsciiKey = "camera";
        static constexpr const char BinaryKey = 'c';

        void readAscii(std::istream& stream);
        void readBinary(std::istream& stream);
        void writeAscii(std::ostream& stream) const;
        void writeBinary(std::ostream& stream) const;

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

        void readAscii(std::istream& stream);
        void readBinary(std::istream& stream);
        void writeAscii(std::ostream& stream) const;
        void writeBinary(std::ostream& stream) const;

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

        void readAscii(std::istream& stream);
        void readBinary(std::istream& stream);
        void writeAscii(std::ostream& stream) const;
        void writeBinary(std::ostream& stream) const;

        Timestamp time;
        std::string script;
        double timestamp;
    };

    struct CommentMessage {
        static constexpr const std::string_view AsciiKey = "#";
        static constexpr const char BinaryKey = '#';

        void readAscii(std::istream& stream);
        void readBinary(std::istream& stream);
        void writeAscii(std::ostream& stream) const;
        void writeBinary(std::ostream& stream) const;

        std::string comment;
    };

    struct Frame : GenericFrame<CameraMessage, TimeMessage, ScriptMessage, CommentMessage>
    {};

    struct SessionRecordingData : GenericSessionRecordingData<Header, Frame> {};

} // namespace version1

inline namespace version2 {
    constexpr const std::string_view Version = "01.00";

    using Timestamp = version1::Timestamp;
    using CameraMessage = version1::CameraMessage;
    using TimeMessage = version1::TimeMessage;
    using CommentMessage = version1::CommentMessage;

    struct ScriptMessage : public version1::ScriptMessage {
        ScriptMessage() = default;
        ScriptMessage(version1::ScriptMessage msg);
        void readBinary(std::istream& stream);
        void writeBinary(std::ostream& stream) const;
    };

    struct Frame : GenericFrame<CameraMessage, TimeMessage, ScriptMessage, CommentMessage>
    {
        Frame() = default;
        explicit Frame(version1::Frame frame);
    };

    struct SessionRecordingData : GenericSessionRecordingData<Header, Frame> {
        SessionRecordingData() = default;
        explicit SessionRecordingData(version1::SessionRecordingData data);
    };

    std::filesystem::path convertSessionRecordingFile(const std::filesystem::path& path);

} // namespace version2

} // namespace openspace::interaction::sessionrecording

#endif // __OPENSPACE_CORE___SESSIONRECORDING_DATA___H__

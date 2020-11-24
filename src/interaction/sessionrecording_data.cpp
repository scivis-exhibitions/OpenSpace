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

#include <openspace/interaction/sessionrecording_data.h>

#include <ghoul/misc/assert.h>
#include <optional>

namespace {
    // Helper structs for the visitor pattern of the std::variant on projections
    template <class... Ts> struct overloaded : Ts... { using Ts::operator()...; };
    template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;
} // namespace

namespace openspace::interaction::sessionrecording {

ConversionError::ConversionError(std::string msg)
    : ghoul::RuntimeError(std::move(msg), "conversionError")
{}

// Common function pointer definitions

using ReadTimeStamp = Timestamps(*)(std::istream& stream, DataMode mode);
using WriteTimeStamp = void(*)(const Timestamps& data, std::ostream& stream,
    DataMode mode);

using ReadCameraKeyframe = CameraKeyframe(*)(std::istream& stream, DataMode mode);
using WriteCameraKeyframe = void(*)(const CameraKeyframe& data, std::ostream& stream,
    DataMode mode);

using ReadTimeKeyframe = TimeKeyframe(*)(std::istream& stream, DataMode mode);
using WriteTimeKeyframe = void(*)(const TimeKeyframe& data, std::ostream& stream,
    DataMode mode);

using ReadScriptMessage = ScriptMessage(*)(std::istream& stream, DataMode mode);
using WriteScriptMessage = void(*)(const ScriptMessage& data, std::ostream& stream,
    DataMode mode);

namespace version1 {

Timestamps readTimeStamps(std::istream& stream, DataMode mode) {
    ghoul_assert(mode != DataMode::Unknown, "Unknown data mode");
    ghoul_assert(stream.good(), "Bad stream");

    Timestamps timestamps;
    if (mode == DataMode::Ascii) {
        stream >> timestamps.timeOs;
        stream >> timestamps.timeRec;
        stream >> timestamps.timeSim;
    }
    else {
        stream.read(reinterpret_cast<char*>(&timestamps.timeOs), sizeof(double));
        stream.read(reinterpret_cast<char*>(&timestamps.timeRec), sizeof(double));
        stream.read(reinterpret_cast<char*>(&timestamps.timeSim), sizeof(double));
    }

    return timestamps;
}

void writeTimeStamps(const Timestamps& data, std::ostream& stream, DataMode mode) {
    ghoul_assert(mode != DataMode::Unknown, "Unknown data mode");
    ghoul_assert(stream.good(), "Bad stream");

    if (mode == DataMode::Ascii) {
        stream << data.timeOs << ' ';
        stream << data.timeRec << ' ';
        stream << data.timeSim << ' ';
    }
    else {
        stream.write(reinterpret_cast<const char*>(&data.timeOs), sizeof(double));
        stream.write(reinterpret_cast<const char*>(&data.timeRec), sizeof(double));
        stream.write(reinterpret_cast<const char*>(&data.timeSim), sizeof(double));
    }
}

CameraKeyframe readCameraKeyframe(std::istream& stream, DataMode mode) {
    ghoul_assert(mode != DataMode::Unknown, "Unknown data mode");
    ghoul_assert(stream.good(), "Bad stream");

    CameraKeyframe frame;
    if (mode == DataMode::Ascii) {
        std::string rotationFollowing;

        stream >> frame._position.x;
        stream >> frame._position.y;
        stream >> frame._position.z;
        stream >> frame._rotation.x;
        stream >> frame._rotation.y;
        stream >> frame._rotation.z;
        stream >> frame._rotation.w;
        stream >> frame._scale;
        stream >> rotationFollowing;
        frame._followNodeRotation = (rotationFollowing == "F");
        stream >> frame._focusNode;
    }
    else {
        //static_assert(typeid(frame._position) == typeid(glm::dvec3));
        static_assert(sizeof(frame._position) == sizeof(glm::dvec3));
        stream.read(reinterpret_cast<char*>(&frame._position), sizeof(glm::dvec3));
        //static_assert(typeid(frame._rotation) == typeid(glm::dquat));
        static_assert(sizeof(frame._rotation) == sizeof(glm::dquat));
        stream.read(reinterpret_cast<char*>(&frame._rotation), sizeof(glm::dquat));
        
        unsigned char b;
        stream.read(reinterpret_cast<char*>(&b), sizeof(unsigned char));
        frame._followNodeRotation = (b == 1);

        int32_t nodeNameLength = 0;
        stream.read(reinterpret_cast<char*>(&nodeNameLength), sizeof(int32_t));
        std::vector<char> nodeNameBuffer(nodeNameLength + 1);
        stream.read(nodeNameBuffer.data(), nodeNameLength);
        frame._focusNode = std::string(nodeNameBuffer.begin(), nodeNameBuffer.end());

        //static_assert(typeid(frame._scale) == typeid(float));
        static_assert(sizeof(frame._scale) == sizeof(float));
        stream.read(reinterpret_cast<char*>(&frame._scale), sizeof(float));

        //static_assert(typeid(frame._timestamp) == typeid(double));
        static_assert(sizeof(frame._timestamp) == sizeof(double));
        stream.read(reinterpret_cast<char*>(&frame._timestamp), sizeof(double));
    }
    return frame;
}

void writeCameraKeyframe(const CameraKeyframe& data, std::ostream& stream, DataMode mode)
{
    ghoul_assert(mode != DataMode::Unknown, "Unknown data mode");
    ghoul_assert(stream.good(), "Bad stream");

    if (mode == DataMode::Ascii) {
        stream << data._position.x << ' ';
        stream << data._position.y << ' ';
        stream << data._position.z << ' ';
        stream << data._rotation.x << ' ';
        stream << data._rotation.y << ' ';
        stream << data._rotation.z << ' ';
        stream << data._rotation.w << ' ';
        stream << data._scale << ' ';
        stream << (data._followNodeRotation ? 'F' : '-') << ' ';
        stream << data._focusNode;
    }
    else {
        stream.write(reinterpret_cast<const char*>(&data._position), sizeof(glm::dvec3));
        stream.write(reinterpret_cast<const char*>(&data._rotation), sizeof(glm::dquat));
        unsigned char b = data._followNodeRotation ? 1 : 0;
        stream.write(reinterpret_cast<const char*>(&b), sizeof(unsigned char));
        int32_t nodeNameLength = static_cast<int32_t>(data._focusNode.size());
        stream.write(reinterpret_cast<const char*>(&nodeNameLength), sizeof(int32_t));
        stream.write(data._focusNode.data(), nodeNameLength);
        stream.write(reinterpret_cast<const char*>(&data._scale), sizeof(float));
        stream.write(reinterpret_cast<const char*>(&data._timestamp), sizeof(double));
    }
}

TimeKeyframe readTimeKeyframe(std::istream& stream, DataMode mode) {
    ghoul_assert(mode != DataMode::Unknown, "Unknown data mode");
    ghoul_assert(stream.good(), "Bad stream");

    TimeKeyframe frame;
    if (mode == DataMode::Ascii) {
        // abock (2020-11-23)   One of those weirdnesses of reusing the struct from the
        // parallel connection is that we ignore the time stamp in the time frame since
        // we have our own in this file format
        stream >> frame._dt;

        std::string paused;
        stream >> paused;
        frame._paused = (paused == "P");

        std::string jump;
        stream >> jump;
        frame._requiresTimeJump = (jump == "J");
    }
    else {
        // @VOLATILE (abock, 2020-11-23)  Yikes!!!  This should be replaced with actually
        // serializing the members bit by bit.  There are two bools that are packed in the
        // middle which are god-only-knows packed when serializing
        stream.read(reinterpret_cast<char*>(&frame), sizeof(TimeKeyframe));


        ////static_assert(typeid(frame._time) == typeid(double));
        //static_assert(sizeof(frame._time) == sizeof(double));
        //stream.read(reinterpret_cast<char*>(&frame._time), sizeof(double));

        ////static_assert(typeid(frame._dt) == typeid(double));
        //static_assert(sizeof(frame._dt) == sizeof(double));
        //stream.read(reinterpret_cast<char*>(&frame._dt), sizeof(double));

        //sizeof(TimeKeyframe);
    }

    return frame;
}

void writeTimeKeyframe(const TimeKeyframe& data, std::ostream& stream, DataMode mode) {
    ghoul_assert(mode != DataMode::Unknown, "Unknown data mode");
    ghoul_assert(stream.good(), "Bad stream");

    if (mode == DataMode::Ascii) {
        stream << data._dt << ' ';
        stream << (data._paused ? 'P' : '-') << ' ';
        stream << (data._paused ? 'J' : '-') << ' ';
    }
    else {
        // @VOLATILE (abock, 2020-11-23) Yikes!!! See above
        stream.write(reinterpret_cast<const char*>(&data), sizeof(TimeKeyframe));
    }
}

ScriptMessage readScriptKeyFrame(std::istream& stream, DataMode mode) {
    ghoul_assert(mode != DataMode::Unknown, "Unknown data mode");
    ghoul_assert(stream.good(), "Bad stream");

    ScriptMessage frame;
    if (mode == DataMode::Ascii) {
        int32_t numScriptLines;
        stream >> numScriptLines;
        if (numScriptLines < 0) {
            // (abock, 2020-11-23)  This should probably throw a ConversionError unless
            // there is a legitimate use case?
            numScriptLines = 0;
        }

        for (int i = 0; i < numScriptLines; ++i) {
            std::string buffer;
            std::getline(stream, buffer);
            const size_t start = buffer.find_first_not_of(" ");
            buffer = buffer.substr(start);
            frame._script.append(buffer);
            if (i < (numScriptLines - 1)) {
                frame._script.append("\n");
            }
        }
    }
    else {
        size_t strLen;
        stream.read(reinterpret_cast<char*>(&strLen), sizeof(size_t));
        std::vector<char> buffer(strLen);
        stream.read(buffer.data(), strLen);
        frame._script = std::string(buffer.begin(), buffer.end());
    }

    return frame;
}

void writeScriptKeyFrame(const ScriptMessage& data, std::ostream& stream, DataMode mode) {
    ghoul_assert(mode != DataMode::Unknown, "Unknown data mode");
    ghoul_assert(stream.good(), "Bad stream");

    if (mode == DataMode::Ascii) {
        const int nLines = static_cast<int>(
            std::count(data._script.begin(), data._script.end(), '\n') + 1
        );
        stream << nLines << ' ';
        stream << data._script;
    }
    else {
        size_t length = static_cast<size_t>(data._script.size());
        stream.write(reinterpret_cast<const char*>(&length), sizeof(size_t));
        stream.write(data._script.data(), length);
    }
}

template <
    DataMode mode,
    ReadTimeStamp readTimeStampFunc, ReadCameraKeyframe readCameraFunc,
    ReadTimeKeyframe readTimeFunc, ReadScriptMessage readScriptFunc
>
std::optional<Frame> readFrame(std::istream& stream) {
    static_assert(mode != DataMode::Unknown, "Unknown data mode");
    ghoul_assert(stream.good(), "Bad stream");

    if constexpr (mode == DataMode::Ascii) {
        constexpr const std::string_view CameraFrame = "camera";
        constexpr const std::string_view TimeFrame = "time";
        constexpr const std::string_view ScriptFrame = "script";
        constexpr const std::string_view CommentFrame = "#";

        std::string entryType;
        stream >> entryType;

        // Check if we reached the end of the file
        if (stream.eof()) {
            return std::nullopt;
        }
        if (entryType == CameraFrame) {
            Frame frame;
            frame.time = readTimeStampFunc(stream, mode);
            CameraKeyframe kf = readCameraFunc(stream, mode);
            // ASCII format does not contain trailing timestamp so add it here
            kf._timestamp = frame.time.timeOs;
            frame.message = kf;
            return frame;
        }
        else if (entryType == TimeFrame) {
            Frame frame;
            frame.time = readTimeStampFunc(stream, mode);
            frame.message = readTimeFunc(stream, mode);
            return frame;
        }
        else if (entryType == ScriptFrame) {
            Frame frame;
            frame.time = readTimeStampFunc(stream, mode);
            frame.message = readScriptFunc(stream, mode);
            return frame;
        }
        else if (entryType == CommentFrame) {
            Frame frame;
            CommentMessage msg;
            std::getline(stream, msg.comment);
            msg.comment = "#" + msg.comment;
            frame.message = msg;
            return frame;
        }
        else {
            throw ConversionError(fmt::format("Unknown frame type {}", entryType));
        }
    }
    else {
        constexpr const unsigned char CameraFrame = 'c';
        constexpr const unsigned char TimeFrame = 't';
        constexpr const unsigned char ScriptFrame = 's';

        unsigned char entryType;
        stream.read(reinterpret_cast<char*>(&entryType), sizeof(unsigned char));

        // Check if we reached the end of the file
        if (stream.eof()) {
            return std::nullopt;
        }
        if (entryType == CameraFrame) {
            Frame frame;
            frame.time = readTimeStampFunc(stream, mode);
            CameraKeyframe kf = readCameraFunc(stream, mode);
            frame.message = kf;
            frame.time.timeOs = kf._timestamp;
            return frame;
        }
        else if (entryType == TimeFrame) {
            Frame frame;
            frame.time = readTimeStampFunc(stream, mode);
            frame.message = readTimeFunc(stream, mode);
            return frame;
        }
        else if (entryType == ScriptFrame) {
            Frame frame;
            frame.time = readTimeStampFunc(stream, mode);
            frame.message = readScriptFunc(stream, mode);
            return frame;
        }
        else {
            throw ConversionError(fmt::format("Unknown frame type {}", entryType));
        }
    }
}

template <
    DataMode mode,
    WriteTimeStamp writeTimeStampFunc, WriteCameraKeyframe writeCameraFunc,
    WriteTimeKeyframe writeTimeFunc, WriteScriptMessage writeScriptFunc
>
void writeFrame(const Frame& data, std::ostream& stream) {
    static_assert(mode != DataMode::Unknown, "Unknown data mode");
    ghoul_assert(stream.good(), "Bad stream");

    if constexpr (mode == DataMode::Ascii) {
        std::visit(overloaded {
            [&](const CameraKeyframe& d) {
                constexpr const std::string_view CameraFrame = "camera";
                stream << CameraFrame << ' ';
                writeTimeStampFunc(data.time, stream, DataMode::Ascii);
                writeCameraFunc(d, stream, DataMode::Ascii);
                stream << '\n';
            },
            [&](const TimeKeyframe& d) {
                constexpr const std::string_view TimeFrame = "time";
                stream << TimeFrame << ' ';
                writeTimeStampFunc(data.time, stream, DataMode::Ascii);
                writeTimeFunc(d, stream, DataMode::Ascii);
                stream << '\n';
            },
            [&](const ScriptMessage& d) {
                constexpr const std::string_view ScriptFrame = "script";
                stream << ScriptFrame << ' ';
                writeTimeStampFunc(data.time, stream, DataMode::Ascii);
                writeScriptFunc(d, stream, DataMode::Ascii);
                stream << '\n';
            },
            [&](const CommentMessage& d) {
                stream << d.comment << '\n';
            }
        }, data.message);
    }
    else {
        std::visit(overloaded {
            [&](const CameraKeyframe& d) {
                constexpr const char CameraFrame = 'c';
                stream.write(&CameraFrame, sizeof(char));
                writeTimeStampFunc(data.time, stream, DataMode::Binary);
                writeCameraFunc(d, stream, DataMode::Binary);
            },
            [&](const TimeKeyframe& d) {
                constexpr const char TimeFrame = 't';
                stream.write(&TimeFrame, sizeof(char));
                writeTimeStampFunc(data.time, stream, DataMode::Binary);
                writeTimeFunc(d, stream, DataMode::Binary);
            },
            [&](const ScriptMessage& d) {
                constexpr const char ScriptFrame = 's';
                stream.write(&ScriptFrame, sizeof(char));
                writeTimeStampFunc(data.time, stream, DataMode::Binary);
                writeScriptFunc(d, stream, DataMode::Binary);
            },
            [](const CommentMessage& d) {
                // We don't currently support comments in binary files
            }
        }, data.message);
    }
}

template <
    DataMode mode,
    ReadTimeStamp readTimeStampFunc, ReadCameraKeyframe readCameraFunc,
    ReadTimeKeyframe readTimeFunc, ReadScriptMessage readScriptFunc
>
SessionRecordingData readSessionRecordingInternal(const std::string& filename) {
    std::ifstream stream;
    if constexpr (mode == DataMode::Binary) {
        stream.open(filename, std::ifstream::binary);
    }
    else {
        stream.open(filename);
    }

    if (!stream.good()) {
        throw ConversionError(fmt::format("Error opening file '{}'", filename));
    }

    // Skip over the header
    {
        constexpr const size_t HeaderSize = sessionrecording::Header::Title.size() +
            sessionrecording::Header::VersionLength +
            + sizeof(char) + sizeof(char); // data mode tag + new line
        std::array<char, HeaderSize> Buffer;
        stream.read(Buffer.data(), HeaderSize);
        Buffer = Buffer;
    }

    SessionRecordingData data;
    if (mode == DataMode::Ascii) {
        int iLine = 0;
        std::string line;
        while (std::getline(stream, line)) {
            iLine++;

            std::istringstream iss(line);
            try {
                std::optional<Frame> frame = readFrame<
                    mode, readTimeStampFunc, readCameraFunc, readTimeFunc, readScriptFunc
                >(
                    iss
                );
                if (frame.has_value()) {
                    data.push_back(std::move(*frame));
                }
            }
            catch (const ConversionError& e) {
                LERRORC(
                    e.component,
                    fmt::format("Error in line {}: {}", iLine, e.message)
                );
                break;
            }
        }
        return data;
    }
    else {
        while (!stream.eof()) {
            try {
                std::optional<Frame> frame = readFrame<
                    mode, readTimeStampFunc, readCameraFunc, readTimeFunc, readScriptFunc
                >(
                    stream
                );
                if (frame.has_value()) {
                    data.push_back(std::move(*frame));
                }
            }
            catch (const ConversionError& e) {
                LERRORC(e.component, e.message);
            }
        }
    }

    return data;
}

template <
    DataMode mode,
    WriteTimeStamp writeTimeStampFunc, WriteCameraKeyframe writeCameraFunc,
    WriteTimeKeyframe writeTimeFunc, WriteScriptMessage writeScriptFunc
>
void writeSessionRecordingInternal(const SessionRecordingData& data,
                                   const std::string& filename,
                                   std::string_view version)
{
    ghoul_assert(mode != DataMode::Unknown, "Unexpected data mode");
    std::ofstream stream;
    if (mode == DataMode::Ascii) {
        stream.open(filename);
    }
    else {
        stream.open(filename, std::ofstream::binary);
    }

    if (!stream.good()) {
        throw ghoul::RuntimeError(fmt::format("Error opening file '{}'", filename));
    }

    if (mode == DataMode::Ascii) {
        stream << std::setprecision(20);
        std::string title = fmt::format("{}{}A\n", Header::Title, version);
        stream.write(title.data(), title.size());
        for (const Frame& frame : data) {
            writeFrame<
                DataMode::Ascii,
                writeTimeStampFunc,
                writeCameraFunc,
                writeTimeFunc,
                writeScriptFunc
            >(frame, stream);
        }
    }
    else {
        std::string title = fmt::format("{}{}B\n", Header::Title, version);
        stream.write(title.data(), title.size());
        for (const Frame& frame : data) {
            writeFrame<
                DataMode::Binary,
                writeTimeStampFunc,
                writeCameraFunc,
                writeTimeFunc,
                writeScriptFunc
            >(frame, stream);
        }
    }
}

DataMode characterToDataMode(unsigned char c) {
    if (c == 'A') {
        return DataMode::Ascii;
    }
    else if (c == 'B') {
        return DataMode::Binary;
    }
    else {
        throw ConversionError(fmt::format("Unknown data mode character '{}' found", c));
    }
}

SessionRecordingData readSessionRecording(const std::string& filename, DataMode mode) {
    if (mode == DataMode::Ascii) {
        return readSessionRecordingInternal<
            DataMode::Ascii,
            &readTimeStamps,
            &readCameraKeyframe,
            &readTimeKeyframe,
            &readScriptKeyFrame
        >(filename);
    }
    else if (mode == DataMode::Binary) {
        return readSessionRecordingInternal<
            DataMode::Binary,
            &readTimeStamps,
            &readCameraKeyframe,
            &readTimeKeyframe,
            &readScriptKeyFrame
        >(filename);
    }
    else {
        throw ghoul::MissingCaseException();
    }
}

} // namespace version1

namespace version2 {

ScriptMessage readScriptKeyFrame(std::istream& stream, [[ maybe_unused ]] DataMode mode) {
    ghoul_assert(mode == DataMode::Binary, "This function only works in binary mode");
    ghoul_assert(stream.good(), "Bad stream");

    uint32_t length;
    stream.read(reinterpret_cast<char*>(&length), sizeof(uint32_t));
    std::vector<char> Buffer(length);
    stream.read(Buffer.data(), length);

    ScriptMessage msg;
    msg._script = std::string(Buffer.begin(), Buffer.end());
    return msg;
}

void writeScriptKeyFrame(const ScriptMessage& data, std::ostream& stream, DataMode mode)
{
    ghoul_assert(mode == DataMode::Binary, "This function only works in binary mode");
    ghoul_assert(stream.good(), "Bad stream");

    uint32_t length = static_cast<uint32_t>(data._script.size());
    stream.write(reinterpret_cast<const char*>(&length), sizeof(uint32_t));
    stream.write(data._script.data(), length);
}

SessionRecordingData updateVersion(version1::SessionRecordingData sessionRecording) {
    // The storage of version 2 and version 1 are the same, only the way the data is
    // loaded has changed
    return sessionRecording;
}

SessionRecordingData readSessionRecording(const std::string& filename, DataMode mode) {
    ghoul_assert(mode != DataMode::Unknown, "Unexpected data mode");

    if (mode == DataMode::Ascii) {
        return version1::readSessionRecordingInternal<
            DataMode::Ascii,
            &version1::readTimeStamps,
            &version1::readCameraKeyframe,
            &version1::readTimeKeyframe,
            &version1::readScriptKeyFrame
        >(filename);
    }
    else {
        return version1::readSessionRecordingInternal<
            DataMode::Binary,
            &version1::readTimeStamps,
            &version1::readCameraKeyframe,
            &version1::readTimeKeyframe,
            &readScriptKeyFrame
        >(filename);
    }
}

void writeSessionRecording(const SessionRecordingData& data, const std::string& filename,
                           DataMode mode)
{
    ghoul_assert(mode != DataMode::Unknown, "Unexpected data mode");

    if (mode == DataMode::Ascii) {
        version1::writeSessionRecordingInternal<
            DataMode::Ascii,
            &version1::writeTimeStamps,
            &version1::writeCameraKeyframe,
            &version1::writeTimeKeyframe,
            &version1::writeScriptKeyFrame
        >(data, filename, Version);
    }
    else {
        version1::writeSessionRecordingInternal<
            DataMode::Binary,
            &version1::writeTimeStamps,
            &version1::writeCameraKeyframe,
            &version1::writeTimeKeyframe,
            &version2::writeScriptKeyFrame
        >(data, filename, Version);
    }
}

} // namespace version2


Header readHeader(std::istream& stream) {
    static std::array<char, Header::Title.size()> TitleBuffer = {};
    stream.read(TitleBuffer.data(), TitleBuffer.size());
    std::string title = std::string(TitleBuffer.begin(), TitleBuffer.end());
    if (title != Header::Title) {
        throw ConversionError("Specified playback file does not contain expected header");
    }

    static std::array<char, Header::VersionLength> VersionBuffer = {};
    stream.read(VersionBuffer.data(), VersionBuffer.size());
    
    Header header;
    header.version = std::string(VersionBuffer.begin(), VersionBuffer.end());
    stream.read(&header.dataMode, sizeof(char));

    // Jump over the newline character
    unsigned char newline;
    stream.read(reinterpret_cast<char*>(&newline), sizeof(unsigned char));
    if (newline == '\r') {
        // Apparently Microsoft still things we need a carriage return character
        stream.read(reinterpret_cast<char*>(&newline), sizeof(unsigned char));
        ghoul_assert(newline == '\n', "Expected newline character not found");
    }
    else {
        ghoul_assert(newline == '\n', "Expected newline character not found");
    }


    return header;
}

std::filesystem::path convertSessionRecordingFile(const std::filesystem::path& path) {
    ghoul_assert(std::filesystem::exists(path), "Path must point to an existing file");

    std::filesystem::path p = path;
    while (true) {
        std::ifstream file(p, std::ifstream::binary);
        Header header = readHeader(file);

        // Update this to a new version if the DataMode enum ever changes and a new
        // character is needed for, for example JSON
        DataMode mode = version1::characterToDataMode(header.dataMode);

        if (header.version == Version) {
            // We have reached the current version
            return p;
        }
        else if (header.version == version1::Version) {
            std::filesystem::path target = path;
            target.replace_filename(fmt::format(
                "{}_{}-{}.{}",
                path.filename().stem().string(),
                version1::Version,
                version2::Version,
                path.extension().string()
            ));

            version1::SessionRecordingData oldData = version1::readSessionRecording(
                p.string(),
                mode
            );
            version2::SessionRecordingData newData = version2::updateVersion(oldData);
            version2::writeSessionRecording(newData, target.string(), mode);
            p = target;
            version2::SessionRecordingData d = version2::readSessionRecording(
                p.string(),
                mode
            );
            d = d;
        }
        else {
            throw ConversionError(fmt::format(
                "Unexpected version number '{}' encountered while converting old session "
                "recording format", header.version
            ));
        }
    }
}

} // namespace openspace::interaction::sessionrecording

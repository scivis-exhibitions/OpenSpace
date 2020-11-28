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

#include <ghoul/logging/logmanager.h>
#include <ghoul/misc/assert.h>
#include <ghoul/fmt.h>
#include <array>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <vector>

namespace {
    // Helper structs for the visitor pattern of the std::variant on projections
    template <class... Ts> struct overloaded : Ts... { using Ts::operator()...; };
    template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

    //template <typename Func, typename... Ts>
    //void for_all(Func&& f, Ts&&... ts) {
    //    using expand = int[];
    //    (void)expand {
    //        0, (f(ts)), 0)...
    //    };
    //}

    //template <class... Types>
    //struct for_each_type;

    //template <typename T>
    //struct for_each_type<std::tuple<T>> {
    //    template <class F, class... Args>
    //    auto operator()(F&& f, Args&&... args) {
    //        f.template operator()<T>(std::forward<Args>(args)...);
    //        return std::forward<F>(f);
    //    }
    //};

} // namespace

namespace openspace::interaction::sessionrecording {

ConversionError::ConversionError(std::string msg)
    : ghoul::RuntimeError(std::move(msg), "ConversionError")
{}


void Header::read(std::istream& stream) {
    static std::array<char, Header::Title.size()> TitleBuffer = {};
    stream.read(TitleBuffer.data(), TitleBuffer.size());
    std::string title = std::string(TitleBuffer.begin(), TitleBuffer.end());
    if (title != Header::Title) {
        throw ConversionError("Specified playback file does not contain expected header");
    }

    static std::array<char, Header::VersionLength> VersionBuffer = {};
    stream.read(VersionBuffer.data(), VersionBuffer.size());

    version = std::string(VersionBuffer.begin(), VersionBuffer.end());
    char mode;
    stream.read(&mode, sizeof(char));
    if (mode == 'A') {
        dataMode = DataMode::Ascii;
    }
    else if (mode == 'B') {
        dataMode = DataMode::Binary;
    }
    else {
        throw ConversionError(fmt::format("Unknown data mode '{}'", mode));
    }

    // Jump over the newline character
    unsigned char newline;
    stream.read(reinterpret_cast<char*>(&newline), sizeof(unsigned char));
    if (newline == '\r') {
        // Apparently Microsoft still thinks we need a carriage return character
        stream.read(reinterpret_cast<char*>(&newline), sizeof(unsigned char));
        ghoul_assert(newline == '\n', "Expected newline character not found");
    }
    else {
        ghoul_assert(newline == '\n', "Expected newline character not found");
    }
}

namespace version1 {

void Timestamp::readAscii(std::istream& stream) {
    ghoul_assert(stream.good(), "Bad stream");

    stream >> timeOs;
    stream >> timeRec;
    stream >> timeSim;
}

void Timestamp::readBinary(std::istream& stream) {
    ghoul_assert(stream.good(), "Bad stream");

    stream.read(reinterpret_cast<char*>(&timeOs), sizeof(double));
    stream.read(reinterpret_cast<char*>(&timeRec), sizeof(double));
    stream.read(reinterpret_cast<char*>(&timeSim), sizeof(double));
}

void Timestamp::writeAscii(std::ostream& stream) const {
    ghoul_assert(stream.good(), "Bad stream");

    stream << timeOs << ' ';
    stream << timeRec << ' ';
    stream << timeSim << ' ';
}

void Timestamp::writeBinary(std::ostream& stream) const {
    ghoul_assert(stream.good(), "Bad stream");

    stream.write(reinterpret_cast<const char*>(&timeOs), sizeof(double));
    stream.write(reinterpret_cast<const char*>(&timeRec), sizeof(double));
    stream.write(reinterpret_cast<const char*>(&timeSim), sizeof(double));
}

void CameraMessage::readAscii(std::istream& stream) {
    ghoul_assert(stream.good(), "Bad stream");

    time.readAscii(stream);
    stream >> position.x;
    stream >> position.y;
    stream >> position.z;
    stream >> rotation.x;
    stream >> rotation.y;
    stream >> rotation.z;
    stream >> rotation.w;
    stream >> scale;
    std::string rotationFollowing;
    stream >> rotationFollowing;
    followNodeRotation = (rotationFollowing == "F");
    stream >> focusNode;

    // ASCII format does not contain trailing timestamp so add it here
    timestamp = time.timeOs;
}

void CameraMessage::readBinary(std::istream& stream) {
    ghoul_assert(stream.good(), "Bad stream");

    time.readBinary(stream);
    static_assert(sizeof(position) == sizeof(glm::dvec3));
    stream.read(reinterpret_cast<char*>(&position), sizeof(glm::dvec3));
    static_assert(sizeof(rotation) == sizeof(glm::dquat));
    stream.read(reinterpret_cast<char*>(&rotation), sizeof(glm::dquat));

    unsigned char b;
    stream.read(reinterpret_cast<char*>(&b), sizeof(unsigned char));
    followNodeRotation = (b == 1);

    int32_t nodeNameLength = 0;
    stream.read(reinterpret_cast<char*>(&nodeNameLength), sizeof(int32_t));
    std::vector<char> nodeNameBuffer(nodeNameLength);
    stream.read(nodeNameBuffer.data(), nodeNameLength);
    focusNode = std::string(nodeNameBuffer.begin(), nodeNameBuffer.end());

    static_assert(sizeof(scale) == sizeof(float));
    stream.read(reinterpret_cast<char*>(&scale), sizeof(float));

    static_assert(sizeof(timestamp) == sizeof(double));
    stream.read(reinterpret_cast<char*>(&timestamp), sizeof(double));
    time.timeOs = timestamp;
}

void CameraMessage::writeAscii(std::ostream& stream) const {
    ghoul_assert(stream.good(), "Bad stream");

    time.writeAscii(stream);
    stream << position.x << ' ';
    stream << position.y << ' ';
    stream << position.z << ' ';
    stream << rotation.x << ' ';
    stream << rotation.y << ' ';
    stream << rotation.z << ' ';
    stream << rotation.w << ' ';
    stream << scale << ' ';
    stream << (followNodeRotation ? 'F' : '-') << ' ';
    stream << focusNode;
}

void CameraMessage::writeBinary(std::ostream& stream) const {
    ghoul_assert(stream.good(), "Bad stream");

    time.writeBinary(stream);
    stream.write(reinterpret_cast<const char*>(&position), sizeof(glm::dvec3));
    stream.write(reinterpret_cast<const char*>(&rotation), sizeof(glm::dquat));
    const unsigned char b = followNodeRotation ? 1 : 0;
    stream.write(reinterpret_cast<const char*>(&b), sizeof(unsigned char));
    const int32_t nodeNameLength = static_cast<int32_t>(focusNode.size());
    stream.write(reinterpret_cast<const char*>(&nodeNameLength), sizeof(int32_t));
    stream.write(focusNode.data(), nodeNameLength);
    stream.write(reinterpret_cast<const char*>(&scale), sizeof(float));
    stream.write(reinterpret_cast<const char*>(&timestamp), sizeof(double));
}

void TimeMessage::readAscii(std::istream& stream) {
    ghoul_assert(stream.good(), "Bad stream");

    time.readAscii(stream);
    // abock (2020-11-23)   One of those weirdnesses of reusing the struct from the
    // parallel connection is that we ignore the time stamp in the time frame since
    // we have our own in this file format
    stream >> dt;

    std::string pausedStr;
    stream >> pausedStr;
    paused = (pausedStr == "P");

    std::string jumpStr;
    stream >> jumpStr;
    requiresTimeJump = (jumpStr == "J");
}

void TimeMessage::readBinary(std::istream& stream) {
    ghoul_assert(stream.good(), "Bad stream");

    time.readBinary(stream);
    // @VOLATILE (abock, 2020-11-23)  Yikes!!!  This should be replaced with actually
    // serializing the members bit by bit.  There are two bools that are packed in the
    // middle which are god-only-knows packed when serializing
    stream.read(reinterpret_cast<char*>(this), sizeof(TimeMessage));
}

void TimeMessage::writeAscii(std::ostream& stream) const {
    ghoul_assert(stream.good(), "Bad stream");

    time.writeAscii(stream);
    stream << dt << ' ';
    stream << (paused ? 'P' : '-') << ' ';
    stream << (paused ? 'J' : '-');
}

void TimeMessage::writeBinary(std::ostream& stream) const {
    ghoul_assert(stream.good(), "Bad stream");

    time.writeBinary(stream);
    // @VOLATILE (abock, 2020-11-23) Yikes!!! See above
    stream.write(reinterpret_cast<const char*>(this), sizeof(TimeMessage));
}

void ScriptMessage::readAscii(std::istream& stream) {
    ghoul_assert(stream.good(), "Bad stream");

    time.readAscii(stream);
    int32_t numScriptLines;
    stream >> numScriptLines;

    for (int i = 0; i < numScriptLines; ++i) {
        std::string buffer;
        std::getline(stream, buffer);
        const size_t start = buffer.find_first_not_of(" ");
        buffer = buffer.substr(start);
        buffer.erase(std::remove(buffer.begin(), buffer.end(), '\r'), buffer.end());
        script.append(buffer);
        if (i < (numScriptLines - 1)) {
            script.append("\n");
        }
    }
}

void ScriptMessage::readBinary(std::istream& stream) {
    ghoul_assert(stream.good(), "Bad stream");

    time.readBinary(stream);
    size_t strLen;
    stream.read(reinterpret_cast<char*>(&strLen), sizeof(size_t));
    std::vector<char> buffer(strLen);
    stream.read(buffer.data(), strLen);
    script = std::string(buffer.begin(), buffer.end());
}

void ScriptMessage::writeAscii(std::ostream& stream) const {
    ghoul_assert(stream.good(), "Bad stream");

    time.writeAscii(stream);
    const int n = static_cast<int>(std::count(script.begin(), script.end(), '\n') + 1);
    stream << n << ' ';
    stream << script;
}

void ScriptMessage::writeBinary(std::ostream& stream) const {
    ghoul_assert(stream.good(), "Bad stream");

    time.writeBinary(stream);
    const size_t length = static_cast<size_t>(script.size());
    stream.write(reinterpret_cast<const char*>(&length), sizeof(size_t));
    stream.write(script.data(), length);
}

void CommentMessage::readAscii(std::istream& stream) {
    ghoul_assert(stream.good(), "Bad stream");
    
    std::getline(stream, comment);
    comment.erase(std::remove(comment.begin(), comment.end(), '\r'), comment.end());
    comment.erase(std::remove(comment.begin(), comment.end(), '\n'), comment.end());
    comment = "#" + comment;
}

void CommentMessage::readBinary(std::istream& stream) {
    ghoul_assert(stream.good(), "Bad stream");

    size_t strLen;
    stream.read(reinterpret_cast<char*>(&strLen), sizeof(size_t));
    std::vector<char> buffer(strLen);
    stream.read(buffer.data(), strLen);
    comment = std::string(buffer.begin(), buffer.end());
}

void CommentMessage::writeAscii(std::ostream& stream) const {
    ghoul_assert(stream.good(), "Bad stream");

    stream << comment;
}

void CommentMessage::writeBinary(std::ostream& stream) const {
    ghoul_assert(stream.good(), "Bad stream");
    
    const size_t length = static_cast<size_t>(comment.size());
    stream.write(reinterpret_cast<const char*>(&length), sizeof(size_t));
    stream.write(comment.data(), length);
}


//template <typename Message/*, typename... MessageTypes*/>
//void handleMessage(Message, /*std::variant<MessageTypes...>& message,*/ std::string_view key, std::istream stream) {
//    if (key == Message::Key) {
//        Message msg;
//        msg.readAscii(stream);
//        //message = msg;
//    }
//}

template <typename... MessageTypes>
bool GenericFrame<MessageTypes...>::readAscii(std::istream& stream) {
    ghoul_assert(stream.good(), "Bad stream");

    std::string entryType;
    stream >> entryType;

    // Check if we reached the end of the file
    if (stream.eof()) {
        return true;
    }

    //for_each_type<std::tuple<MessageTypes...>>{}(
    //    [&](auto& tuple, std::string_view key, std::istream& stream) {
    //        if (key == tuple.AsciiKey) {
    //            decltype(tuple) msg;
    //            msg.readAscii(stream);
    //            message = msg;
    //        }

    //    },
    //    message, entryType, stream
    //);

    std::string_view key = entryType;
    std::apply(
        [this, key, &stream](auto& ...x) { (..., handleMessage(x, key, stream)); },
        std::tuple<MessageTypes...>()
    );

    //for_all([&](std::string_view type, auto& msg) {
    //    if (type == )
    //}, MessageTypes...);

    if (entryType == CameraMessage::AsciiKey) {
        CameraMessage msg;
        msg.readAscii(stream);
        message = msg;
    }
    else if (entryType == TimeMessage::AsciiKey) {
        TimeMessage msg;
        msg.readAscii(stream);
        message = msg;
    }
    else if (entryType == ScriptMessage::AsciiKey) {
        ScriptMessage msg;
        msg.readAscii(stream);
        message = msg;
    }
    else if (entryType == CommentMessage::AsciiKey) {
        CommentMessage msg;
        msg.readAscii(stream);
        message = msg;
    }
    else {
        throw ConversionError(fmt::format("Unknown frame type {}", entryType));
    }

    return false;
}

bool Frame::readBinary(std::istream& stream) {
    ghoul_assert(stream.good(), "Bad stream");

    unsigned char entryType;
    stream.read(reinterpret_cast<char*>(&entryType), sizeof(unsigned char));

    // Check if we reached the end of the file
    if (stream.eof()) {
        return true;
    }
    if (entryType == CameraMessage::BinaryKey) {
        CameraMessage msg;
        msg.readBinary(stream);
        message = msg;
    }
    else if (entryType == TimeMessage::BinaryKey) {
        TimeMessage msg;
        msg.readBinary(stream);
        message = msg;
    }
    else if (entryType == ScriptMessage::BinaryKey) {
        ScriptMessage msg;
        msg.readBinary(stream);
        message = msg;
    }
    else if (entryType == CommentMessage::BinaryKey) {
        CommentMessage msg;
        msg.readBinary(stream);
        message = msg;
    }
    else {
        throw ConversionError(fmt::format("Unknown frame type {}", entryType));
    }

    return false;
}

void Frame::writeAscii(std::ostream& stream) const {
    ghoul_assert(stream.good(), "Bad stream");

    std::visit([&](auto message) {
        stream << message.AsciiKey << ' ';
        message.writeAscii(stream);
        stream << '\n';
    }, message);
}

void Frame::writeBinary(std::ostream& stream) const {
    ghoul_assert(stream.good(), "Bad stream");

    std::visit([&](auto message) {
        stream.write(&message.BinaryKey, sizeof(char));
        message.writeBinary(stream);
    }, message);
}

void SessionRecordingData::read(const std::string& filename) {
    std::ifstream stream;
    stream.open(filename, std::ifstream::binary);

    if (!stream.good()) {
        throw ConversionError(fmt::format("Error opening file '{}'", filename));
    }

    header.read(stream);

    SessionRecordingData data;
    if (header.dataMode == DataMode::Ascii) {
        int iLine = 0;
        std::string line;
        while (std::getline(stream, line)) {
            iLine++;

            std::istringstream iss(line);
            try {
                Frame frame;
                bool eof = frame.readAscii(iss);
                if (eof) {
                    break;
                }
                frames.push_back(std::move(frame));
            }
            catch (const ConversionError& e) {
                LERRORC(
                    e.component,
                    fmt::format("Error in line {}: {}", iLine, e.message)
                );
                break;
            }
        }
    }
    else {
        while (!stream.eof()) {
            try {
                Frame frame;
                bool eof = frame.readBinary(stream);
                if (eof) {
                    break;
                }
                frames.push_back(std::move(frame));
            }
            catch (const ConversionError& e) {
                LERRORC(e.component, e.message);
            }
        }
    }
}

void SessionRecordingData::write(const std::string& filename, DataMode mode) const {
    std::ofstream stream;
    if (mode == DataMode::Ascii) {
        stream.open(filename);
        stream << std::setprecision(20);
    }
    else {
        stream.open(filename, std::ofstream::binary);
    }

    if (!stream.good()) {
        throw ghoul::RuntimeError(fmt::format("Error opening file '{}'", filename));
    }

    if (mode == DataMode::Ascii) {
        std::string title = fmt::format("{}{}A\n", Header::Title, Version);
        stream << title;
    }
    else {
        std::string title = fmt::format("{}{}B\n", Header::Title, Version);
        stream.write(title.data(), title.size());
    }

    for (const Frame& frame : frames) {
        if (mode == DataMode::Ascii) {
            frame.writeAscii(stream);
        }
        else {
            frame.writeBinary(stream);
        }
    }
}

} // namespace version1

namespace version2 {

ScriptMessage::ScriptMessage(const version1::ScriptMessage& msg)
    : version1::ScriptMessage(msg)
{}

void ScriptMessage::readBinary(std::istream& stream) {
    ghoul_assert(stream.good(), "Bad stream");

    time.readBinary(stream);
    uint32_t length;
    stream.read(reinterpret_cast<char*>(&length), sizeof(uint32_t));
    std::vector<char> Buffer(length);
    stream.read(Buffer.data(), length);

    script = std::string(Buffer.begin(), Buffer.end());
}

void ScriptMessage::writeBinary(std::ostream& stream) const {
    ghoul_assert(stream.good(), "Bad stream");

    time.writeBinary(stream);
    const uint32_t length = static_cast<uint32_t>(script.size());
    stream.write(reinterpret_cast<const char*>(&length), sizeof(uint32_t));
    stream.write(script.data(), length);
}

Frame::Frame(const version1::Frame& frame) {
    std::visit([&](auto a) { message = a; }, frame.message);
}

bool Frame::readAscii(std::istream& stream) {
    ghoul_assert(stream.good(), "Bad stream");

    constexpr const std::string_view CameraFrame = "camera";
    constexpr const std::string_view TimeFrame = "time";
    constexpr const std::string_view ScriptFrame = "script";
    constexpr const std::string_view CommentFrame = "#";

    std::string entryType;
    stream >> entryType;

    // Check if we reached the end of the file
    if (stream.eof()) {
        return true;
    }

    if (entryType == CameraFrame) {
        CameraMessage msg;
        msg.readAscii(stream);
        message = msg;
    }
    else if (entryType == TimeFrame) {
        TimeMessage msg;
        msg.readAscii(stream);
        message = msg;
    }
    else if (entryType == ScriptFrame) {
        ScriptMessage msg;
        msg.readAscii(stream);
        message = msg;
    }
    else if (entryType == CommentFrame) {
        CommentMessage msg;
        msg.readAscii(stream);
        message = msg;
    }
    else {
        throw ConversionError(fmt::format("Unknown frame type {}", entryType));
    }

    return false;
}

bool Frame::readBinary(std::istream& stream) {
    ghoul_assert(stream.good(), "Bad stream");

    constexpr const unsigned char CameraFrame = 'c';
    constexpr const unsigned char TimeFrame = 't';
    constexpr const unsigned char ScriptFrame = 's';
    constexpr const unsigned char CommentFrame = '#';

    unsigned char entryType;
    stream.read(reinterpret_cast<char*>(&entryType), sizeof(unsigned char));

    // Check if we reached the end of the file
    if (stream.eof()) {
        return true;
    }
    if (entryType == CameraFrame) {
        CameraMessage msg;
        msg.readBinary(stream);
        message = msg;
    }
    else if (entryType == TimeFrame) {
        TimeMessage msg;
        msg.readBinary(stream);
        message = msg;
    }
    else if (entryType == ScriptFrame) {
        ScriptMessage msg;
        msg.readBinary(stream);
        message = msg;
    }
    else if (entryType == CommentFrame) {
        CommentMessage msg;
        msg.readBinary(stream);
        message = msg;
    }
    else {
        throw ConversionError(fmt::format("Unknown frame type {}", entryType));
    }

    return false;
}

void Frame::writeAscii(std::ostream& stream) const {
    ghoul_assert(stream.good(), "Bad stream");

    std::visit(overloaded{
        [&](const CameraMessage& d) {
            constexpr const std::string_view CameraFrame = "camera";
            stream << CameraFrame << ' ';
            d.writeAscii(stream);
            stream << '\n';
        },
        [&](const TimeMessage& d) {
            constexpr const std::string_view TimeFrame = "time";
            stream << TimeFrame << ' ';
            d.writeAscii(stream);
            stream << '\n';
        },
        [&](const ScriptMessage& d) {
            constexpr const std::string_view ScriptFrame = "script";
            stream << ScriptFrame << ' ';
            d.writeAscii(stream);
            stream << '\n';
        },
        [&](const CommentMessage& d) {
            d.writeAscii(stream);
            stream << '\n';
        }
    }, message);
}

void Frame::writeBinary(std::ostream& stream) const {
    ghoul_assert(stream.good(), "Bad stream");

    std::visit(overloaded{
        [&](const CameraMessage& d) {
            constexpr const char CameraFrame = 'c';
            stream.write(&CameraFrame, sizeof(char));
            d.writeBinary(stream);
        },
        [&](const TimeMessage& d) {
            constexpr const char TimeFrame = 't';
            stream.write(&TimeFrame, sizeof(char));
            d.writeBinary(stream);
        },
        [&](const ScriptMessage& d) {
            constexpr const char ScriptFrame = 's';
            stream.write(&ScriptFrame, sizeof(char));
            d.writeBinary(stream);
        },
        [&](const CommentMessage& d) {
            constexpr const char CommentFrame = '#';
            stream.write(&CommentFrame, sizeof(char));
            d.writeBinary(stream);
        }
    }, message);
}

SessionRecordingData::SessionRecordingData(const version1::SessionRecordingData& data) {
    header = data.header;
    header.version = Version;
    frames.reserve(data.frames.size());
    for (const version1::Frame& f : data.frames) {
        frames.push_back(f);
    }
}

void SessionRecordingData::read(const std::string& filename) {
    std::ifstream stream;
    stream.open(filename, std::ifstream::binary);

    if (!stream.good()) {
        throw ConversionError(fmt::format("Error opening file '{}'", filename));
    }

    header.read(stream);

    SessionRecordingData data;
    if (header.dataMode == DataMode::Ascii) {
        int iLine = 0;
        std::string line;
        while (std::getline(stream, line)) {
            iLine++;

            std::istringstream iss(line);
            try {
                Frame frame;
                bool eof = frame.readAscii(iss);
                if (eof) {
                    break;
                }
                frames.push_back(std::move(frame));
            }
            catch (const ConversionError& e) {
                LERRORC(
                    e.component,
                    fmt::format("Error in line {}: {}", iLine, e.message)
                );
                break;
            }
        }
    }
    else {
        while (!stream.eof()) {
            try {
                Frame frame;
                bool eof = frame.readBinary(stream);
                if (eof) {
                    break;
                }
                frames.push_back(std::move(frame));
            }
            catch (const ConversionError& e) {
                LERRORC(e.component, e.message);
            }
        }
    }
}

void SessionRecordingData::write(const std::string& filename, DataMode mode) const {
    std::ofstream stream;
    if (mode == DataMode::Ascii) {
        stream.open(filename);
        stream << std::setprecision(20);
    }
    else {
        stream.open(filename, std::ofstream::binary);
    }

    if (!stream.good()) {
        throw ghoul::RuntimeError(fmt::format("Error opening file '{}'", filename));
    }

    if (mode == DataMode::Ascii) {
        std::string title = fmt::format("{}{}A\n", Header::Title, header.version);
        stream << title;
    }
    else {
        std::string title = fmt::format("{}{}B\n", Header::Title, header.version);
        stream.write(title.data(), title.size());
    }

    for (const Frame& frame : frames) {
        if (mode == DataMode::Ascii) {
            frame.writeAscii(stream);
        }
        else {
            frame.writeBinary(stream);
        }
    }
}

std::filesystem::path convertSessionRecordingFile(const std::filesystem::path& path) {
    ghoul_assert(std::filesystem::exists(path), "Path must point to an existing file");

    std::filesystem::path p = path;
    while (true) {
        std::ifstream file(p, std::ifstream::binary);
        Header header;
        header.read(file);
        file.seekg(0);

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

            version1::SessionRecordingData oldData;
            oldData.read(p.string());
            version2::SessionRecordingData newData = oldData;
            //version2::SessionRecordingData newData = version2::updateVersion(oldData);
            newData.write(target.string(), header.dataMode);
            p = target;
        }
        else {
            throw ConversionError(fmt::format(
                "Unexpected version number '{}' encountered while converting old session "
                "recording format", header.version
            ));
        }
    }
}

} // namespace version2

} // namespace::interaction::sessionrecording

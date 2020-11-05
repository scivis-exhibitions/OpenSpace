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

#include <modules/space/translation/horizonstranslation.h>

#include <openspace/documentation/documentation.h>
#include <openspace/documentation/verifier.h>
#include <openspace/util/updatestructures.h>
#include <ghoul/fmt.h>
#include <ghoul/filesystem/cachemanager.h>
#include <ghoul/filesystem/filesystem.h>
#include <ghoul/logging/logmanager.h>
#include <ghoul/lua/ghoul_lua.h>
#include <ghoul/lua/lua_helper.h>
#include <fstream>

namespace {
    constexpr const char* _loggerCat = "HorizonsTranslation";
} // namespace

namespace {
    constexpr openspace::properties::Property::PropertyInfo HorizonsTextFileInfo = {
        "HorizonsTextFile",
        "Horizons Text File",
        "This value is the path to the text file generated by Horizons with observer "
        "range and Galactiv longitude and latitude for different timestamps."
    };
} // namespace

namespace openspace {

documentation::Documentation HorizonsTranslation::Documentation() {
    using namespace documentation;
    return {
        "Horizons Translation",
        "base_transform_translation_horizons",
        {
            {
                "Type",
                new StringEqualVerifier("HorizonsTranslation"),
                Optional::No
            },
            {
                HorizonsTextFileInfo.identifier,
                new StringVerifier,
                Optional::No,
                HorizonsTextFileInfo.description
            }
        }
    };
}


HorizonsTranslation::HorizonsTranslation()
    : _horizonsTextFile(HorizonsTextFileInfo)
{
    addProperty(_horizonsTextFile);

    _horizonsTextFile.onChange([&](){
        requireUpdate();
        _fileHandle = std::make_unique<ghoul::filesystem::File>(_horizonsTextFile);
        _fileHandle->setCallback([&](const ghoul::filesystem::File&) {
             requireUpdate();
             notifyObservers();
         });
        loadData();
    });
}

HorizonsTranslation::HorizonsTranslation(const ghoul::Dictionary& dictionary)
    : HorizonsTranslation()
{
    documentation::testSpecificationAndThrow(
        Documentation(),
        dictionary,
        "HorizonsTranslation"
    );

    _horizonsTextFile = absPath(
        dictionary.value<std::string>(HorizonsTextFileInfo.identifier)
    );
}

glm::dvec3 HorizonsTranslation::position(const UpdateData& data) const {
    glm::dvec3 interpolatedPos = glm::dvec3(0.0);

    auto lastBefore = _timeline.lastKeyframeBefore(data.time.j2000Seconds(), true);
    auto firstAfter = _timeline.firstKeyframeAfter(data.time.j2000Seconds(), false);
    if (lastBefore && firstAfter) {
        // We're inbetween first and last value.
        double timelineDiff = firstAfter->timestamp - lastBefore->timestamp;
        double timeDiff = data.time.j2000Seconds() - lastBefore->timestamp;
        double diff = (timelineDiff > DBL_EPSILON) ? timeDiff / timelineDiff : 0.0;

        glm::dvec3 dir = firstAfter->data - lastBefore->data;
        interpolatedPos = lastBefore->data + dir * diff;
    }
    else if (lastBefore) {
        // Requesting a time after last value. Return last known position.
        interpolatedPos = lastBefore->data;
    }
    else if (firstAfter) {
        // Requesting a time before first value. Return last known position.
        interpolatedPos = firstAfter->data;
    }

    return interpolatedPos;
}

void HorizonsTranslation::loadData() {
    std::string file = _horizonsTextFile;
    if (!FileSys.fileExists(absPath(file))) {
        return;
    }

    std::string cachedFile = FileSys.cacheManager()->cachedFilename(
        file,
        ghoul::filesystem::CacheManager::Persistent::Yes
    );

    bool hasCachedFile = FileSys.fileExists(cachedFile);
    if (hasCachedFile) {
        LINFO(fmt::format("Cached file '{}' used for Horizon file '{}'", cachedFile, file));

        bool success = loadCachedFile(cachedFile);
        if (success) {
            return;
        }
        else {
            FileSys.cacheManager()->removeCacheFile(file);
            // Intentional fall-through to the 'else' computation to generate the cache
            // file for the next run
        }
    }
    else {
        LINFO(fmt::format("Cache for Horizon file '{}' not found", file));
    }
    LINFO(fmt::format("Loading Horizon file '{}'", file));

    readHorizonsTextFile();

    LINFO("Saving cache");
    saveCachedFile(cachedFile);
}

void HorizonsTranslation::readHorizonsTextFile() {
    std::ifstream fileStream(_horizonsTextFile);

    if (!fileStream.good()) {
        LERROR(fmt::format(
            "Failed to open Horizons text file '{}'", _horizonsTextFile
        ));
        return;
    }

    // The beginning of a Horizons file has a header with a lot of information about the
    // query that we do not care about. Ignore everything until data starts, including
    // the row marked by $$SOE (i.e. Start Of Ephemerides).
    std::string line;
    while (line[0] != '$') {
        std::getline(fileStream, line);
    }

    // Read data line by line until $$EOE (i.e. End Of Ephemerides).
    // Skip the rest of the file.
    std::getline(fileStream, line);
    while (line[0] != '$') {
        std::stringstream str(line);
        std::string date;
        std::string time;
        float range = 0;
        float gLon = 0;
        float gLat = 0;

        // File is structured by:
        // YYYY-MM-DD
        // HH:MM:SS
        // Range-to-observer (km)
        // Range-delta (km/s) -- suppressed!
        // Galactic Longitude (degrees)
        // Galactic Latitude (degrees)
        str >> date >> time >> range >> gLon >> gLat;

        // Convert date and time to seconds after 2000
        // and pos to Galactic positions in meter from Observer.
        std::string timeString = date + " " + time;
        double timeInJ2000 = Time::convertTime(timeString);
        glm::dvec3 gPos = glm::dvec3(
            1000 * range * cos(glm::radians(gLat)) * cos(glm::radians(gLon)),
            1000 * range * cos(glm::radians(gLat)) * sin(glm::radians(gLon)),
            1000 * range * sin(glm::radians(gLat))
        );

        // Add position to stored timeline.
        _timeline.addKeyframe(timeInJ2000, std::move(gPos));

        std::getline(fileStream, line);
    }
    fileStream.close();
}

bool HorizonsTranslation::loadCachedFile(const std::string& file) {
    std::ifstream fileStream(file, std::ifstream::binary);

    if (!fileStream.good()) {
        LERROR(fmt::format("Error opening file '{}' for loading cache file", file));
        return false;
    }

    // Read how many keyframes to read
    int32_t nKeyframes = 0;

    fileStream.read(reinterpret_cast<char*>(&nKeyframes), sizeof(int32_t));
    if (nKeyframes == 0) {
        throw ghoul::RuntimeError("Error reading cache: No values were loaded");
    }

    // Read the values in same order as they were written
    for (int i = 0; i < nKeyframes; ++i) {
        double timestamp, x, y, z;
        glm::dvec3 gPos;

        // Timestamp
        fileStream.read(reinterpret_cast<char*>(&timestamp), sizeof(double));

        // Position vector components
        fileStream.read(reinterpret_cast<char*>(&x), sizeof(double));
        fileStream.read(reinterpret_cast<char*>(&y), sizeof(double));
        fileStream.read(reinterpret_cast<char*>(&z), sizeof(double));

        // Recreate the position vector
        gPos = glm::dvec3(x, y, z);

        // Add keyframe in timeline
        _timeline.addKeyframe(timestamp, std::move(gPos));
    }

    return fileStream.good();
}

void HorizonsTranslation::saveCachedFile(const std::string& file) const {
    std::ofstream fileStream(file, std::ofstream::binary);
    if (!fileStream.good()) {
        LERROR(fmt::format("Error opening file '{}' for save cache file", file));
        return;
    }

    // Write how many keyframes are to be written
    int32_t nKeyframes = static_cast<int32_t>(_timeline.nKeyframes());
    if (nKeyframes == 0) {
        throw ghoul::RuntimeError("Error writing cache: No values were loaded");
    }
    fileStream.write(reinterpret_cast<const char*>(&nKeyframes), sizeof(int32_t));

    // Write data
    std::deque<Keyframe<glm::dvec3>> keyframes = _timeline.keyframes();
    for (int i = 0; i < nKeyframes; i++) {
        // First write timestamp
        fileStream.write(reinterpret_cast<const char*>(
            &keyframes[i].timestamp),
            sizeof(double)
        );

        // and then the components of the position vector one by one
        fileStream.write(reinterpret_cast<const char*>(
            &keyframes[i].data.x),
            sizeof(double)
        );
        fileStream.write(reinterpret_cast<const char*>(
            &keyframes[i].data.y),
            sizeof(double)
        );
        fileStream.write(reinterpret_cast<const char*>(
            &keyframes[i].data.z),
            sizeof(double)
        );
    }
}
} // namespace openspace

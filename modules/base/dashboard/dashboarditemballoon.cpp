/*****************************************************************************************
 *                                                                                       *
 * OpenSpace                                                                             *
 *                                                                                       *
 * Copyright (c) 2014-2021                                                               *
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

#include <modules/base/dashboard/dashboarditemballoon.h>

#include <openspace/documentation/documentation.h>
#include <openspace/documentation/verifier.h>
#include <openspace/engine/globals.h>
#include <openspace/util/spicemanager.h>
#include <openspace/util/timemanager.h>
#include <ghoul/font/font.h>
#include <ghoul/font/fontmanager.h>
#include <ghoul/font/fontrenderer.h>
#include <ghoul/logging/logmanager.h>
#include <ghoul/misc/profiling.h>
#include <iostream>
#include <ghoul/filesystem/filesystem.h>
#include <ghoul/filesystem/cachemanager.h>
#include <fstream>
#include <ghoul/lua/ghoul_lua.h>
#include <ghoul/lua/lua_helper.h>
#include <ghoul/fmt.h>
#include <string>
namespace {
    constexpr openspace::properties::Property::PropertyInfo FormatStringInfo = {
        "FormatString",
        "Format String",
        "The format text describing how this dashboard item renders it's text. This text "
        "must contain exactly one {} which is a placeholder that will contain the date"
    };

    constexpr openspace::properties::Property::PropertyInfo TimeFormatInfo = {
        "TimeFormat",
        "TimeFormatsy",
        "The format string used for formatting the date/time before being passed to the "
        "string in FormatString. See "
        "https://naif.jpl.nasa.gov/pub/naif/toolkit_docs/C/cspice/timout_c.html for full "
        "information about how to structure this format"
    };

    constexpr openspace::properties::Property::PropertyInfo BalloonTextFileInfo = {
        "BalloonTextFile",
        "Balloon Text File",
        "something something"
    };
    
    struct [[codegen::Dictionary(DashboardItemBalloon)]] Parameters {
        // [[codegen::verbatim(FormatStringInfo.description)]]
        std::optional<std::string> formatString;

        // [[codegen::verbatim(TimeFormatInfo.description)]]
        std::optional<std::string> timeFormat;

        // [[codegen::verbatim(BalloonTextFileInfo.description)]]
        std::optional<std::string> balloonTextFile;
    };
#include "dashboarditemballoon_codegen.cpp"
} // namespace

namespace openspace {

documentation::Documentation DashboardItemBalloon::Documentation() {
    documentation::Documentation doc = codegen::doc<Parameters>();
    doc.id = "base_dashboarditem_balloon";
    return doc;
}

DashboardItemBalloon::DashboardItemBalloon(const ghoul::Dictionary& dictionary)
    : DashboardTextItem(dictionary, 15.f)
    , _formatString(FormatStringInfo, "Date: {} UTC")
    , _timeFormat(TimeFormatInfo, "YYYY MON DDTHR:MN:SC.### ::RND")
    , _balloonTextFile(BalloonTextFileInfo)
{
    const Parameters p = codegen::bake<Parameters>(dictionary);

    _balloonTextFile = absPath(p.balloonTextFile.value_or("det blev fel"));
    _formatString = p.formatString.value_or(_formatString);
    addProperty(_formatString);
    addProperty(_balloonTextFile);
    std::cout << "är jag här inne nu?" << std::endl;
    loadData();
    _balloonTextFile.onChange([&]() {

        //requireUpdate();
        _fileHandle = std::make_unique<ghoul::filesystem::File>(_balloonTextFile);
        _fileHandle->setCallback([&](const ghoul::filesystem::File&) {
            //requireUpdate();
            //notifyObservers();
            });
        loadData();
        });
}
void DashboardItemBalloon::loadData() {
    std::cout << "kommer jag in hit?" << std::endl;
    std::string file = _balloonTextFile;
    if (!FileSys.fileExists(absPath(file))) {
        return;
    }

    std::string cachedFile = FileSys.cacheManager()->cachedFilename(
        file,
        ghoul::filesystem::CacheManager::Persistent::Yes
    );
    /*bool hasCachedFile = FileSys.fileExists(cachedFile);
    if (hasCachedFile) {
        //LINFO(fmt::format(
            //"Cached file '{}' used for Horizon file '{}'", cachedFile, file
        //));

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
        //LINFO(fmt::format("Cache for Horizon file '{}' not found", file));
    }*/
    //LINFO(fmt::format("Loading Horizon file '{}'", file));

    readHorizonsTextFile();

    //LINFO("Saving cache");
    saveCachedFile(cachedFile);
}

bool DashboardItemBalloon::loadCachedFile(const std::string& file) {
    std::ifstream fileStream(file, std::ifstream::binary);

    if (!fileStream.good()) {
        //LERROR(fmt::format("Error opening file '{}' for loading cache file", file));
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
        _timeline1.addKeyframe(timestamp, std::move(gPos));
    }

    return fileStream.good();
}

void DashboardItemBalloon::saveCachedFile(const std::string& file) const {
    std::ofstream fileStream(file, std::ofstream::binary);
    if (!fileStream.good()) {
        //LERROR(fmt::format("Error opening file '{}' for save cache file", file));
        return;
    }

    // Write how many keyframes are to be written
    int32_t nKeyframes = static_cast<int32_t>(_timeline1.nKeyframes());
    if (nKeyframes == 0) {
        throw ghoul::RuntimeError("Error writing cache: No values were loaded");
    }
    fileStream.write(reinterpret_cast<const char*>(&nKeyframes), sizeof(int32_t));

    // Write data
    std::deque<Keyframe<glm::dvec3>> keyframes = _timeline1.keyframes();
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
void DashboardItemBalloon::render(glm::vec2& penPosition) {
    ZoneScoped
    std::string height = "";
    auto lastBefore1 = _timeline1.lastKeyframeBefore(global::timeManager->time().j2000Seconds(), true);
    auto firstAfter1 = _timeline1.firstKeyframeAfter(global::timeManager->time().j2000Seconds(), false);
    auto lastBefore2 = _timeline2.lastKeyframeBefore(global::timeManager->time().j2000Seconds(), true);
    auto firstAfter2 = _timeline2.firstKeyframeAfter(global::timeManager->time().j2000Seconds(), false);
    auto lastBefore3 = _timeline3.lastKeyframeBefore(global::timeManager->time().j2000Seconds(), true);
    auto firstAfter3 = _timeline3.firstKeyframeAfter(global::timeManager->time().j2000Seconds(), false);
    auto lastBefore4 = _timeline4.lastKeyframeBefore(global::timeManager->time().j2000Seconds(), true);
    auto firstAfter4 = _timeline4.firstKeyframeAfter(global::timeManager->time().j2000Seconds(), false);
    std::string dataString = "height: ";
    if (lastBefore1 && firstAfter1) {
        double timelineDiff = firstAfter1->timestamp - lastBefore1->timestamp;
        double timeDiff = global::timeManager->time().j2000Seconds() - lastBefore1->timestamp;
        double diff = (timelineDiff > DBL_EPSILON) ? timeDiff / timelineDiff : 0.0;
        dataString.append(std::to_string((1 - diff)*lastBefore1->data.x + (diff)*firstAfter1->data.x));
        dataString.append("\n Longitude: ");
        dataString.append(std::to_string((1 - diff)*lastBefore1->data.y + (diff)*firstAfter1->data.y));
        dataString.append("\n Latitude: ");
        dataString.append(std::to_string((1 - diff)*lastBefore1->data.z + (diff)*firstAfter1->data.z));
        dataString.append("\n Pressure: ");
        dataString.append(std::to_string((1 - diff)*lastBefore2->data.x + (diff)*firstAfter2->data.x));
        dataString.append("\n Temperature: ");
        dataString.append(std::to_string((1 - diff)*lastBefore2->data.y + (diff)*firstAfter2->data.y));
        dataString.append("\n Q: ");
        dataString.append(std::to_string((1 - diff)*lastBefore2->data.z + (diff)*firstAfter2->data.z));
        dataString.append("\n TH: ");
        dataString.append(std::to_string((1 - diff)*lastBefore3->data.x + (diff)*firstAfter3->data.x));
        dataString.append("\n Distance: ");
        dataString.append(std::to_string((1 - diff)*lastBefore3->data.y + (diff)*firstAfter3->data.y));
        dataString.append("\n RH: ");
        dataString.append(std::to_string((1 - diff)*lastBefore3->data.z + (diff)*firstAfter3->data.z));
        dataString.append("\n PS: ");
        dataString.append(std::to_string((1 - diff)*lastBefore4->data.x + (diff)*firstAfter4->data.x));
    }
    else if (lastBefore1) {
        dataString.append(std::to_string(lastBefore1->data.x));
        dataString.append("\n Longitude: ");
        dataString.append(std::to_string(lastBefore1->data.y));
        dataString.append("\n Latitude: ");
        dataString.append(std::to_string(lastBefore1->data.z));
        dataString.append("\n Pressure: ");
        dataString.append(std::to_string(lastBefore2->data.x));
        dataString.append("\n Temperature: ");
        dataString.append(std::to_string(lastBefore2->data.y));
        dataString.append("\n Q: ");
        dataString.append(std::to_string(lastBefore2->data.z));
        dataString.append("\n TH: ");
        dataString.append(std::to_string(lastBefore3->data.x));
        dataString.append("\n Distance: ");
        dataString.append(std::to_string(lastBefore3->data.y));
        dataString.append("\n RH: ");
        dataString.append(std::to_string(lastBefore3->data.z));
        dataString.append("\n PS: ");
        dataString.append(std::to_string(lastBefore4->data.x));
    }
    else if (firstAfter1) {
        dataString.append(std::to_string(firstAfter1->data.x));
        dataString.append("\n Longitude: ");
        dataString.append(std::to_string(firstAfter1->data.y));
        dataString.append("\n Latitude: ");
        dataString.append(std::to_string(firstAfter1->data.z));
        dataString.append("\n Pressure: ");
        dataString.append(std::to_string(firstAfter2->data.x));
        dataString.append("\n Temperature: ");
        dataString.append(std::to_string(firstAfter2->data.y));
        dataString.append("\n Q: ");
        dataString.append(std::to_string(firstAfter2->data.z));
        dataString.append("\n TH: ");
        dataString.append(std::to_string(firstAfter3->data.x));
        dataString.append("\n Distance: ");
        dataString.append(std::to_string(firstAfter3->data.y));
        dataString.append("\n RH: ");
        dataString.append(std::to_string(firstAfter3->data.z));
        dataString.append("\n PS: ");
        dataString.append(std::to_string(firstAfter4->data.x));
    }
    try {
        RenderFont(*_font, penPosition, fmt::format(dataString, height));
    }
    catch (const fmt::format_error&) {
        LERRORC("DashboardItemballoon", "Illegal format string");
    }
    penPosition.y -= _font->height();
}

void DashboardItemBalloon::readHorizonsTextFile() {
    std::ifstream fileStream(_balloonTextFile);
    std::cout << "kommer jag in hit?" << std::endl;
    if (!fileStream.good()) {
        //LERROR(fmt::format(
          //  "Failed to open Horizons text file '{}'", _horizonsTextFile
        //));
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
        float height = 0;
        float gLon = 0;
        float gLat = 0;
        float P = 0;
        float T = 0;
        float Q = 0;
        float TH = 0;
        float DIST = 0;
        float RH = 0;
        float PS = 0;

        // File is structured by:
        // YYYY-MM-DD
        // HH:MM:SS
        // Range-to-observer (km)
        // Range-delta (km/s) -- suppressed!
        // Galactic Longitude (degrees)
        // Galactic Latitude (degrees)
        str >> date >> time >> height >> gLon >> gLat >> P >> T >> Q >> TH >> DIST>>RH>>PS;

        // Convert date and time to seconds after 2000
        // and pos to Galactic positions in meter from Observer.
        std::string timeString = date + " " + time;
        double timeInJ2000 = Time::convertTime(timeString);
        /*glm::dvec3 gPos = glm::dvec3(
            1000 * range * cos(glm::radians(gLat)) * cos(glm::radians(gLon)),
            1000 * range * cos(glm::radians(gLat)) * sin(glm::radians(gLon)),
            1000 * range * sin(glm::radians(gLat))
        );*/
        std::cout  <<" "<< date << " " << time << " " << height << " " << gLon << " " << gLat << " " << P << " " << T << " " << Q << " " << TH << " " << DIST << " " << RH << " " << PS<<std::endl;
        glm::dvec3 heightlonlat = glm::dvec3(height, gLon, gLat);
        glm::dvec3 ptq = glm::dvec3(P, T, Q);
        glm::dvec3 thdistrh = glm::dvec3(TH, DIST, RH);
        glm::dvec3 ps = glm::dvec3(PS, 3, 5);
        // Add position to stored timeline.
        _timeline1.addKeyframe(timeInJ2000, heightlonlat);
        _timeline2.addKeyframe(timeInJ2000, ptq);
        _timeline3.addKeyframe(timeInJ2000, thdistrh);
        _timeline4.addKeyframe(timeInJ2000, ps);
        std::getline(fileStream, line);
    }
    fileStream.close();
}
glm::vec2 DashboardItemBalloon::size() const {
    ZoneScoped

    std::string_view time = global::timeManager->time().UTC();
    return _font->boundingBox(fmt::format(_formatString.value().c_str(), time));
}

} // namespace openspace

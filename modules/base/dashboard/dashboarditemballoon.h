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

#ifndef __OPENSPACE_MODULE_BASE___DASHBOARDITEMBALLOON___H__
#define __OPENSPACE_MODULE_BASE___DASHBOARDITEMBALLOON___H__
#include <openspace/scene/translation.h>
#include <openspace/rendering/dashboardtextitem.h>

#include <openspace/properties/stringproperty.h>
#include <ghoul/filesystem/file.h>
#include <openspace/util/timeline.h>
namespace openspace {

namespace documentation { struct Documentation; }

class DashboardItemBalloon : public DashboardTextItem {
public:
    DashboardItemBalloon(const ghoul::Dictionary& dictionary);
    ~DashboardItemBalloon() = default;

    void render(glm::vec2& penPosition) override;

    glm::vec2 size() const override;

    static documentation::Documentation Documentation();

private:
    void loadData();
    void readHorizonsTextFile();
    bool loadCachedFile(const std::string& file);
    void saveCachedFile(const std::string& file) const;


    properties::StringProperty _balloonTextFile;
    properties::StringProperty _formatString;
    properties::StringProperty _timeFormat;

    std::unique_ptr<ghoul::filesystem::File> _fileHandle;

    Timeline<glm::dvec3> _timeline1;
    Timeline<glm::dvec3> _timeline2;
    Timeline<glm::dvec3> _timeline3;
    Timeline<glm::dvec3> _timeline4;
};

} // namespace openspace

#endif // __OPENSPACE_MODULE_BASE___DASHBOARDITEMBALLOON___H__

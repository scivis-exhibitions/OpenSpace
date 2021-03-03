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

#include <modules/exoplanetsexperttool/dataviewer.h>

#include <modules/imgui/include/imgui_include.h>
#include <ghoul/filesystem/filesystem.h>
#include <ghoul/logging/logmanager.h>
#include <fstream>

namespace {
    constexpr const char _loggerCat[] = "ExoplanetsDataViewer";

    // TODO: make these file paths a property or something
    constexpr const char dataFilePath[] =
        "${MODULE_EXOPLANETSEXPERTTOOL}/data/data.bin";

    constexpr const char lookupTablePath[] =
        "${MODULE_EXOPLANETSEXPERTTOOL}/data/lookup.txt";
}

namespace openspace::exoplanets::gui {

DataViewer::DataViewer(std::string identifier, std::string guiName)
    : properties::PropertyOwner({ std::move(identifier), std::move(guiName) })
{}

void DataViewer::loadData() {
    std::ifstream data(absPath(dataFilePath), std::ios::in | std::ios::binary);
    if (!data.good()) {
        LERROR(fmt::format("Failed to open data file: '{}'", dataFilePath));
        return;
    }

    std::ifstream lut(absPath(lookupTablePath));
    if (!lut.good()) {
        LERROR(fmt::format("Failed to open look-up table: '{}'", lookupTablePath));
        return;
    }

    _data.clear();

    // TODO: resize data vector based on number of planets

    // Iterate through the lookup-table file and add each planet
    std::string line;
    while (std::getline(lut, line)) {
        std::istringstream ss(line);
        std::string name;
        std::getline(ss, name, ',');

        std::string location_s;
        std::getline(ss, location_s, ',');
        long location = std::stol(location_s.c_str());

        std::string hostName;
        std::getline(ss, hostName, ',');

        // Not used for now, but is in the lookup-table
        std::string component;
        std::getline(ss, component);

        ExoplanetRecord e;
        data.seekg(location);
        data.read(reinterpret_cast<char*>(&e), sizeof(ExoplanetRecord));

        ExoplanetGuiItem item{ e, name, hostName };
        _data.push_back(item);
    }
}

void DataViewer::render() {
    static ImGuiTableFlags flags =
        ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY | ImGuiTableFlags_BordersV
        | ImGuiTableFlags_Reorderable | ImGuiTableFlags_Hideable | ImGuiTableFlags_BordersOuter;
        //| ImGuiTableFlags_Sortable | ImGuiTableFlags_SortMulti
        //| ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter;

    int nColumns = 4;

    if (ImGui::BeginTable("exoplanets_table", nColumns, flags)) {
        ImGui::TableSetupColumn("Name");
        ImGui::TableSetupColumn("Host");
        ImGui::TableSetupColumn("ESM");
        ImGui::TableSetupColumn("TSM");
        ImGui::TableSetupScrollFreeze(0, 1); // Make header always visible
        ImGui::TableHeadersRow();

        for (int row = 0; row < _data.size(); row++)
        {
            const ExoplanetGuiItem& item = _data[row];
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(item.planetName.c_str());
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(item.hostName.c_str());
            ImGui::TableNextColumn();
            ImGui::Text("%f", item.esm);
            ImGui::TableNextColumn();
            ImGui::Text("%f", item.tsm);
        }
        ImGui::EndTable();
    }

}

} // namespace openspace::gui

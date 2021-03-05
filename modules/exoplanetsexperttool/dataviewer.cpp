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
#include <algorithm>
#include <fstream>

namespace {
    constexpr const char _loggerCat[] = "ExoplanetsDataViewer";

    // TODO: make these file paths a property or something
    constexpr const char dataFilePath[] =
        "${MODULE_EXOPLANETSEXPERTTOOL}/data/data.bin";

    constexpr const char lookupTablePath[] =
        "${MODULE_EXOPLANETSEXPERTTOOL}/data/lookup.txt";

    auto caseInsensitiveLessThan(std::string lhs, std::string rhs) {
        std::transform(lhs.begin(), lhs.end(), lhs.begin(), std::tolower);
        std::transform(rhs.begin(), rhs.end(), rhs.begin(), std::tolower);
        return (lhs < rhs);
    }
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
    renderTable();
}

void DataViewer::renderTable() {
    static ImGuiTableFlags flags =
        ImGuiTableFlags_ScrollX | ImGuiTableFlags_ScrollY
        | ImGuiTableFlags_BordersV | ImGuiTableFlags_BordersOuter
        | ImGuiTableFlags_Reorderable | ImGuiTableFlags_Hideable
        | ImGuiTableFlags_Sortable | ImGuiTableFlags_Resizable
        | ImGuiTableFlags_RowBg | ImGuiTableFlags_NoSavedSettings;

    enum ColumnID {
        Name,
        Host,
        NPlanets,
        ESM,
        TSM,
        PlanetRadius,
        PlanetTemperature,
        StarTemperature
    };

    static struct Column {
        const char* name;
        ColumnID id;
        ImGuiTableColumnFlags flags = 0;
    };

    // TODO: filter the data based on user inputs
    static std::vector<ExoplanetGuiItem> data = _data;

    const std::vector<Column> columns = {
        { "Name", ColumnID::Name, ImGuiTableColumnFlags_DefaultSort },
        { "Host", ColumnID::Host },
        { "Number planets", ColumnID::NPlanets },
        { "ESM", ColumnID::ESM },
        { "TSM", ColumnID::TSM },
        { "Planet radius (Earth radii)", ColumnID::PlanetRadius },
        { "Planet equilibrium temp. (Kelvin)", ColumnID::PlanetTemperature },
        { "Star effective temp. (Kelvin)", ColumnID::StarTemperature }
    };
    const int nColumns = columns.size();

    if (ImGui::BeginTable("exoplanets_table", nColumns, flags)) {
        // Header
        for (auto c : columns) {
            ImGui::TableSetupColumn(c.name, c.flags, 0.f, c.id);
        }
        ImGui::TableSetupScrollFreeze(0, 1); // Make header always visible
        ImGui::TableHeadersRow();

        // Sorting
        if (ImGuiTableSortSpecs* sortSpecs = ImGui::TableGetSortSpecs()) {
            if (sortSpecs->SpecsDirty) {
                // TODO: sort based on sort specs (column, order, etc)
                auto compare = [&sortSpecs](const ExoplanetGuiItem& lhs,
                                            const ExoplanetGuiItem& rhs) -> bool
                {
                    bool flip = (sortSpecs->Specs->SortDirection
                                 == ImGuiSortDirection_Descending);

                    const ExoplanetGuiItem& l = flip ? rhs : lhs;
                    const ExoplanetGuiItem& r = flip ? lhs : rhs;

                    switch (sortSpecs->Specs->ColumnUserID) {
                    case ColumnID::Name:
                        return caseInsensitiveLessThan(l.planetName, r.planetName);
                    case ColumnID::Host:
                        return caseInsensitiveLessThan(l.hostName, r.hostName);
                    case ColumnID::NPlanets:
                        return l.nPlanets < r.nPlanets;
                    case ColumnID::ESM:
                        return l.esm < r.esm;
                    case ColumnID::TSM:
                        return l.tsm < r.tsm;
                    case ColumnID::PlanetRadius:
                        return l.radius.value < r.radius.value;
                    case ColumnID::PlanetTemperature:
                        return l.eqilibriumTemp.value < r.eqilibriumTemp.value;
                    case ColumnID::StarTemperature:
                        return l.starEffectiveTemp.value < r.starEffectiveTemp.value;
                    }
                };

                std::sort(data.begin(), data.end(), compare);
                sortSpecs->SpecsDirty = false;
            }
        }

        // Rows
        for (int row = 0; row < _data.size(); row++) {
            const ExoplanetGuiItem& item = data[row];
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(item.planetName.c_str());
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(item.hostName.c_str());
            ImGui::TableNextColumn();
            ImGui::Text("%d", item.nPlanets);
            ImGui::TableNextColumn();
            ImGui::Text("%.2f", item.esm);
            ImGui::TableNextColumn();
            ImGui::Text("%.2f", item.tsm);
            ImGui::TableNextColumn();
            ImGui::Text("%.2f", item.radius.value);
            ImGui::TableNextColumn();
            ImGui::Text("%.0f", item.eqilibriumTemp.value);
            ImGui::TableNextColumn();
            ImGui::Text("%.0f", item.starEffectiveTemp.value);
        }
        ImGui::EndTable();
    }
}

} // namespace openspace::gui

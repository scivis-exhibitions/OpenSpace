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

#include <modules/exoplanetsexperttool/rendering/renderablepointdata.h>
#include <modules/imgui/include/imgui_include.h>
#include <openspace/engine/globals.h>
#include <openspace/query/query.h>
#include <openspace/rendering/renderable.h>
#include <openspace/rendering/renderengine.h>
#include <openspace/scene/scene.h>
#include <openspace/scripting/scriptengine.h>
#include <ghoul/filesystem/filesystem.h>
#include <ghoul/glm.h>
#include <ghoul/logging/logmanager.h>
#include <ghoul/misc/dictionary.h>
#include <ghoul/misc/dictionaryluaformatter.h>
#include <algorithm>
#include <fstream>

namespace {
    constexpr const char _loggerCat[] = "ExoplanetsDataViewer";

    auto caseInsensitiveLessThan(std::string lhs, std::string rhs) {
        std::transform(lhs.begin(), lhs.end(), lhs.begin(), std::tolower);
        std::transform(rhs.begin(), rhs.end(), rhs.begin(), std::tolower);
        return (lhs < rhs);
    }
}

namespace openspace::exoplanets::gui {

DataViewer::DataViewer(std::string identifier, std::string guiName)
    : properties::PropertyOwner({ std::move(identifier), std::move(guiName) })
    , _allPointsIdentifier("AllExoplanets")
    , _selectedPointsIdentifier("SelectedExoplanets")
{
    _fullData = _dataLoader.loadData();

    // Create points from data (should be a function depending on the indices?)
    ghoul::Dictionary positions;

    int counter = 1;
    for (auto item : _fullData) {
        if (item.position.has_value()) {
            std::string index = fmt::format("[{}]", counter);
            positions.setValue<glm::dvec3>(index, item.position.value());
            counter++;
        }
        // TODO: will it be a problem that we don't add all points?
    }

    using namespace std::string_literals;

    ghoul::Dictionary gui;
    gui.setValue("Name", "All Exoplanets"s);
    gui.setValue("Path", "/ExoplanetsTool"s);

    ghoul::Dictionary renderable;
    renderable.setValue("Type", "RenderablePointData"s);
    renderable.setValue("Color", glm::dvec3(0.9, 1.0, 0.5));
    renderable.setValue("Size", 10.0);
    renderable.setValue("Positions", positions);

    ghoul::Dictionary node;
    node.setValue("Identifier", _allPointsIdentifier);
    node.setValue("Renderable", renderable);
    node.setValue("GUI", gui);

    openspace::global::scriptEngine->queueScript(
        fmt::format("openspace.addSceneGraphNode({})", ghoul::formatLua(node)),
        scripting::ScriptEngine::RemoteScripting::Yes
    );

    ghoul::Dictionary guiSelected;
    guiSelected.setValue("Name", "Selected Exoplanets"s);
    guiSelected.setValue("Path", "/ExoplanetsTool"s);

    ghoul::Dictionary renderableSelected;
    renderableSelected.setValue("Type", "RenderablePointData"s);
    renderableSelected.setValue("Color", glm::dvec3(0.0, 1.0, 0.8));
    renderableSelected.setValue("Size", 40.0);
    renderableSelected.setValue("Positions", ghoul::Dictionary());

    ghoul::Dictionary nodeSelected;
    nodeSelected.setValue("Identifier", _selectedPointsIdentifier);
    nodeSelected.setValue("Renderable", renderableSelected);
    nodeSelected.setValue("GUI", guiSelected);

    openspace::global::scriptEngine->queueScript(
        fmt::format("openspace.addSceneGraphNode({})", ghoul::formatLua(nodeSelected)),
        scripting::ScriptEngine::RemoteScripting::Yes
    );
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
        | ImGuiTableFlags_RowBg;

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

    // TODO: filter the data based on user inputs
    static std::vector<ExoplanetItem> data = _fullData;
    static ImVector<int> selection;
    bool selectionChanged = false;

    if (ImGui::BeginTable("exoplanets_table", nColumns, flags)) {
        // Header
        for (auto c : columns) {
            auto colFlags = c.flags | ImGuiTableColumnFlags_PreferSortDescending;
            ImGui::TableSetupColumn(c.name, colFlags, 0.f, c.id);
        }
        ImGui::TableSetupScrollFreeze(0, 1); // Make header always visible
        ImGui::TableHeadersRow();

        // Sorting
        if (ImGuiTableSortSpecs* sortSpecs = ImGui::TableGetSortSpecs()) {
            if (sortSpecs->SpecsDirty) {
                auto compare = [&sortSpecs](const ExoplanetItem& lhs,
                    const ExoplanetItem& rhs) -> bool
                {
                    bool flip = (sortSpecs->Specs->SortDirection
                        == ImGuiSortDirection_Descending);

                    const ExoplanetItem& l = flip ? rhs : lhs;
                    const ExoplanetItem& r = flip ? lhs : rhs;

                    switch (sortSpecs->Specs->ColumnUserID) {
                    case ColumnID::Name:
                        return !caseInsensitiveLessThan(l.planetName, r.planetName);
                    case ColumnID::Host:
                        return !caseInsensitiveLessThan(l.hostName, r.hostName);
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
        for (int row = 0; row < data.size(); row++) {
            const ExoplanetItem& item = data[row];
            const bool itemIsSelected = selection.contains(row);

            ImGuiSelectableFlags selectableFlags = ImGuiSelectableFlags_SpanAllColumns
                | ImGuiSelectableFlags_AllowItemOverlap;

            ImGui::TableNextColumn();

            if (ImGui::Selectable(item.planetName.c_str(), itemIsSelected, selectableFlags)) {
                if (ImGui::GetIO().KeyCtrl) {
                    if (itemIsSelected) {
                        selection.find_erase_unsorted(row);
                    }
                    else {
                        selection.push_back(row);
                    }
                }
                else {
                    selection.clear();
                    selection.push_back(row);
                }

                selectionChanged = true;
            }

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

        // Update selection renderable
        if (selectionChanged) {
            SceneGraphNode* node = sceneGraphNode(_selectedPointsIdentifier);
            if (!node) {
                LDEBUG(fmt::format("Renderable with identifier '{}' not yet created", _selectedPointsIdentifier));
                return;
            }

            RenderablePointData* r = dynamic_cast<RenderablePointData*>(node->renderable());

            std::vector<glm::dvec3> positions;
            positions.reserve(selection.size());
            for (int row : selection) {
                const ExoplanetItem& item = data[row];
                if (item.position.has_value()) {
                    positions.push_back(item.position.value());
                }
            }

            r->updateData(positions);
        }
    }
}

} // namespace openspace::gui

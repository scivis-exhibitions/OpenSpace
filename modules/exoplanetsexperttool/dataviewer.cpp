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

    bool caseInsensitiveLessThan(std::string lhs, std::string rhs) {
        std::transform(lhs.begin(), lhs.end(), lhs.begin(), std::tolower);
        std::transform(rhs.begin(), rhs.end(), rhs.begin(), std::tolower);
        return (lhs < rhs);
    }

    bool compareValues(double lhs, double rhs) {
        if (std::isnan(lhs)) {
            // also includes rhs is nan, in which case the roder does no matter
            return true;
        }

        // rhs is nan, but not lhs
        if (std::isnan(rhs)) {
            return false;
        }
        return (lhs < rhs);
    }
}

namespace openspace::exoplanets::gui {

DataViewer::DataViewer(std::string identifier, std::string guiName)
    : properties::PropertyOwner({ std::move(identifier), std::move(guiName) })
    , _allPointsIdentifier("AllExoplanets")
    , _selectedPointsIdentifier("SelectedExoplanets")
{
    _data = _dataLoader.loadData();

    _tableData.reserve(_data.size());
    for (size_t i = 0; i < _data.size(); i++) {
        _tableData.push_back({ i });
    }
}

void DataViewer::initialize() {
    initializeRenderables();
}

void DataViewer::initializeRenderables() {
    using namespace std::string_literals;

    ghoul::Dictionary positions;
    int counter = 1;
    for (auto item : _data) {
        if (item.position.has_value()) {
            std::string index = fmt::format("[{}]", counter);
            positions.setValue<glm::dvec3>(index, item.position.value());
            counter++;
        }
        // TODO: will it be a problem that we don't add all points?
    }

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

    const ImGuiTableColumnFlags hide = ImGuiTableColumnFlags_DefaultHide;

    enum ColumnID {
        Name,
        Host,
        DiscoveryYear,
        NPlanets,
        NStars,
        ESM,
        TSM,
        PlanetRadius,
        PlanetTemperature,
        PlanetMass,
        SurfaceGravity,
        SemiMajorAxis,
        Eccentricity,
        Period,
        Inclination,
        StarTemperature,
        StarRadius,
        MagnitudeJ,
        MagnitudeK,
        Distance
    };

    struct Column {
        const char* name;
        ColumnID id;
        ImGuiTableColumnFlags flags = 0;
    };

    const std::vector<Column> columns = {
        { "Name", ColumnID::Name, ImGuiTableColumnFlags_DefaultSort },
        { "Host", ColumnID::Host },
        { "Year of discovery", ColumnID::DiscoveryYear },
        { "Num. planets", ColumnID::NPlanets },
        { "Num. stars ", ColumnID::NStars },
        { "ESM", ColumnID::ESM },
        { "TSM", ColumnID::TSM },
        { "Planet radius (Earth radii)", ColumnID::PlanetRadius },
        { "Planet equilibrium temp. (K)", ColumnID::PlanetTemperature },
        { "Mass", ColumnID::PlanetMass },
        { "Surface Gravity (m/s^2)", ColumnID::SurfaceGravity },
        // Orbits
        { "Semi-major axis (AU)", ColumnID::SemiMajorAxis },
        { "Eccentricity", ColumnID::Eccentricity },
        { "Orbit period", ColumnID::Period },
        { "Inclination", ColumnID::Inclination },
        // Star
        { "Star effective temp. (K)", ColumnID::StarTemperature },
        { "Star radius (Solar)", ColumnID::StarRadius },
        { "MagJ", ColumnID::MagnitudeJ },
        { "MagK", ColumnID::MagnitudeK },
        { "Distance (Parsec)", ColumnID::Distance}
    };
    const int nColumns = static_cast<int>(columns.size());

    bool selectionChanged = false;
    bool filterChanged = false;

    // Filtering
    filterChanged |= ImGui::Checkbox("Hide nan TSM", &_hideNanTsm);
    ImGui::SameLine();
    filterChanged |= ImGui::Checkbox("Hide nan ESM", &_hideNanEsm);
    ImGui::SameLine();
    filterChanged |= ImGui::Checkbox("Only multi-planet", &_showOnlyMultiPlanetSystems);

    static ImGuiTextFilter filter;
    filterChanged |= filter.Draw();

    ImGui::SameLine();
    if (ImGui::Button("Clear")) {
        filter.Clear();
        filterChanged = true;
    }

    if (filterChanged) {
        for (TableItem& f : _tableData) {
            const ExoplanetItem& d = _data[f.index];

            // TODO: implement filter per column

            bool filtered = _hideNanTsm && std::isnan(d.esm);
            filtered |= _hideNanEsm && std::isnan(d.tsm);
            filtered |= _showOnlyMultiPlanetSystems && !d.multiSystemFlag;
            filtered |= !(filter.PassFilter(d.planetName.c_str()));

            f.isVisible = !filtered;

            // If a filtered item is selected, remove it from selection
            if (filtered) {
                auto found = std::find(_selection.begin(), _selection.end(), f.index);
                const bool itemIsSelected = found != _selection.end();

                if (itemIsSelected) {
                    _selection.erase(found);
                    selectionChanged = true;
                }
            }
        }
    }

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
                auto compare = [&sortSpecs, this](const TableItem& lhs,
                                                  const TableItem& rhs) -> bool
                {
                    ImGuiSortDirection sortDir = sortSpecs->Specs->SortDirection;
                    bool flip = (sortDir == ImGuiSortDirection_Descending);

                    const ExoplanetItem& l = flip ? _data[rhs.index] : _data[lhs.index];
                    const ExoplanetItem& r = flip ? _data[lhs.index] : _data[rhs.index];

                    switch (sortSpecs->Specs->ColumnUserID) {
                    case ColumnID::Name:
                        return !caseInsensitiveLessThan(l.planetName, r.planetName);
                    case ColumnID::Host:
                        return !caseInsensitiveLessThan(l.hostName, r.hostName);
                    case ColumnID::DiscoveryYear:
                        return l.discoveryYear < r.discoveryYear;
                    case ColumnID::NPlanets:
                        return l.nPlanets < r.nPlanets;
                    case ColumnID::NStars:
                        return l.nStars < r.nStars;
                    case ColumnID::ESM:
                        return compareValues(l.esm, r.esm);
                    case ColumnID::TSM:
                        return compareValues(l.tsm, r.tsm);
                    case ColumnID::PlanetRadius:
                        return compareValues(l.radius.value, r.radius.value);
                    case ColumnID::PlanetTemperature:
                        return compareValues(
                            l.eqilibriumTemp.value,
                            r.eqilibriumTemp.value
                        );
                    case ColumnID::PlanetMass:
                        return compareValues(l.mass.value, r.mass.value);
                    case ColumnID::SurfaceGravity:
                        return compareValues(
                            l.surfaceGravity.value,
                            r.surfaceGravity.value
                        );
                    // Orbits
                    case ColumnID::SemiMajorAxis:
                        return compareValues(
                            l.semiMajorAxis.value,
                            r.semiMajorAxis.value
                        );
                    case ColumnID::Eccentricity:
                        return compareValues(l.eccentricity.value, r.eccentricity.value);
                    case ColumnID::Period:
                        return compareValues(l.period.value, r.period.value);

                    case ColumnID::Inclination:
                        return compareValues(l.inclination.value, r.inclination.value);
                    // Star
                    case ColumnID::StarTemperature:
                        return compareValues(
                            l.starEffectiveTemp.value,
                            r.starEffectiveTemp.value
                        );
                    case ColumnID::StarRadius:
                        return compareValues(l.starRadius.value, r.starRadius.value);
                    case ColumnID::MagnitudeJ:
                        return compareValues(l.magnitudeJ.value, r.magnitudeJ.value);
                    case ColumnID::MagnitudeK:
                        return compareValues(l.magnitudeK.value, r.magnitudeK.value);
                    case ColumnID::Distance:
                        return compareValues(l.distance.value, r.distance.value);
                    default:
                        LWARNING(fmt::format("Sorting for column {} not defined"));
                        return false;
                    }
                };

                std::sort(_tableData.begin(), _tableData.end(), compare);
                sortSpecs->SpecsDirty = false;
            }
        }

        // Rows
        for (size_t row = 0; row < _tableData.size(); row++) {
            if (!_tableData[row].isVisible) {
                continue; // filtered
            }

            const size_t index = _tableData[row].index;
            const ExoplanetItem& item = _data[index];

            auto found = std::find(_selection.begin(), _selection.end(), index);
            const bool itemIsSelected = found != _selection.end();

            ImGuiSelectableFlags selectableFlags = ImGuiSelectableFlags_SpanAllColumns
                | ImGuiSelectableFlags_AllowItemOverlap;

            ImGui::TableNextColumn();

            if (ImGui::Selectable(item.planetName.c_str(), itemIsSelected, selectableFlags)) {
                if (ImGui::GetIO().KeyCtrl) {
                    if (itemIsSelected) {
                        _selection.erase(found);
                    }
                    else {
                        _selection.push_back(index);
                    }
                }
                else {
                    _selection.clear();
                    _selection.push_back(index);
                }

                selectionChanged = true;
            }

            // OBS! Same order as columns list above
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(item.hostName.c_str());
            ImGui::TableNextColumn();
            ImGui::Text("%d", item.discoveryYear);
            ImGui::TableNextColumn();
            ImGui::Text("%d", item.nPlanets);
            ImGui::TableNextColumn();
            ImGui::Text("%d", item.nStars);
            ImGui::TableNextColumn();
            ImGui::Text("%.2f", item.esm);
            ImGui::TableNextColumn();
            ImGui::Text("%.2f", item.tsm);
            ImGui::TableNextColumn();
            ImGui::Text("%.2f", item.radius.value);
            ImGui::TableNextColumn();
            ImGui::Text("%.0f", item.eqilibriumTemp.value);
            ImGui::TableNextColumn();
            ImGui::Text("%.2f", item.mass.value);
            ImGui::TableNextColumn();
            ImGui::Text("%.2f", item.surfaceGravity.value);
            // Orbital
            ImGui::TableNextColumn();
            ImGui::Text("%.2f", item.semiMajorAxis.value);
            ImGui::TableNextColumn();
            ImGui::Text("%.2f", item.eccentricity.value);
            ImGui::TableNextColumn();
            ImGui::Text("%.2f", item.period.value);
            ImGui::TableNextColumn();
            ImGui::Text("%.2f", item.inclination.value);
            // Star
            ImGui::TableNextColumn();
            ImGui::Text("%.0f", item.starEffectiveTemp.value);
            ImGui::TableNextColumn();
            ImGui::Text("%.2f", item.starRadius.value);
            ImGui::TableNextColumn();
            ImGui::Text("%.2f", item.magnitudeJ.value);
            ImGui::TableNextColumn();
            ImGui::Text("%.2f", item.magnitudeK.value);
            ImGui::TableNextColumn();
            ImGui::Text("%.2f", item.distance.value);
        }
        ImGui::EndTable();

        // Update selection renderable
        if (selectionChanged) {
            SceneGraphNode* node = sceneGraphNode(_selectedPointsIdentifier);
            if (!node) {
                LDEBUG(fmt::format(
                    "Renderable with identifier '{}' not yet created",
                    _selectedPointsIdentifier
                ));
                return;
            }

            RenderablePointData* r = dynamic_cast<RenderablePointData*>(node->renderable());

            std::vector<glm::dvec3> positions;
            positions.reserve(_selection.size());
            for (size_t index : _selection) {
                const ExoplanetItem& item = _data[index];
                if (item.position.has_value()) {
                    positions.push_back(item.position.value());
                }
            }

            r->updateData(positions);
        }
    }
}

} // namespace openspace::gui

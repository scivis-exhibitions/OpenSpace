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

    bool caseInsensitiveLessThan(const char* lhs, const char* rhs) {
        int res = _stricmp(lhs, rhs);
        return res < 0;
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
        return lhs < rhs;
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

    struct Column {
        const char* name;
        ColumnID id;
        const char* format = "%s";
        ImGuiTableColumnFlags flags = 0;
    };

    const std::vector<Column> columns = {
        { "Name", ColumnID::Name, "%s", ImGuiTableColumnFlags_DefaultSort },
        { "Host", ColumnID::Host },
        { "Year of discovery", ColumnID::DiscoveryYear, "%d" },
        { "Num. planets", ColumnID::NPlanets, "%d" },
        { "Num. stars ", ColumnID::NStars, "%d" },
        { "ESM", ColumnID::ESM, "%.2f" },
        { "TSM", ColumnID::TSM, "%.2f" },
        { "Planet radius (Earth radii)", ColumnID::PlanetRadius, "%.2f" },
        { "Planet equilibrium temp. (K)", ColumnID::PlanetTemperature, "%.0f" },
        { "Mass", ColumnID::PlanetMass, "%.2f" },
        { "Surface Gravity (m/s^2)", ColumnID::SurfaceGravity, "%.2f" },
        // Orbits
        { "Semi-major axis (AU)", ColumnID::SemiMajorAxis, "%.2f" },
        { "Eccentricity", ColumnID::Eccentricity, "%.2f" },
        { "Orbit period", ColumnID::Period, "%.2f" },
        { "Inclination", ColumnID::Inclination, "%.2f" },
        // Star
        { "Star effective temp. (K)", ColumnID::StarTemperature, "%.0f" },
        { "Star radius (Solar)", ColumnID::StarRadius, "%.2f" },
        { "MagJ", ColumnID::MagnitudeJ, "%.2f" },
        { "MagK", ColumnID::MagnitudeK, "%.2f" },
        { "Distance (Parsec)", ColumnID::Distance, "%.2f" }
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

            bool filtered = _hideNanTsm && std::isnan(d.tsm);
            filtered |= _hideNanEsm && std::isnan(d.esm);
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

                    ColumnID col = static_cast<ColumnID>(sortSpecs->Specs->ColumnUserID);

                    return compareColumnValues(col, l, r);
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

            ImGuiSelectableFlags selectableFlags = ImGuiSelectableFlags_SpanAllColumns
                | ImGuiSelectableFlags_AllowItemOverlap;

            auto found = std::find(_selection.begin(), _selection.end(), index);
            const bool itemIsSelected = found != _selection.end();

            for (const Column col : columns) {
                ImGui::TableNextColumn();

                if (col.id == ColumnID::Name) {
                    bool changed = ImGui::Selectable(
                        item.planetName.c_str(),
                        itemIsSelected,
                        selectableFlags
                    );

                    if (changed) {
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
                    continue;
                }

                renderColumnValue(col.id, col.format, item);
            }
        }
        ImGui::EndTable();

        // Update selection renderable
        if (selectionChanged) {
            //SceneGraphNode* node = sceneGraphNode(_selectedPointsIdentifier);
            //if (!node) {
            //    LDEBUG(fmt::format(
            //        "Renderable with identifier '{}' not yet created",
            //        _selectedPointsIdentifier
            //    ));
            //    return;
            //}

            //RenderablePointData* r = dynamic_cast<RenderablePointData*>(node->renderable());

            //std::vector<glm::dvec3> positions;
            //positions.reserve(_selection.size());
            //for (size_t index : _selection) {
            //    const ExoplanetItem& item = _data[index];
            //    if (item.position.has_value()) {
            //        positions.push_back(item.position.value());
            //    }
            //}

            ////r->updateData(positions);
        }
    }
}

void DataViewer::renderColumnValue(ColumnID column, const char* format,
                                   const ExoplanetItem& item)
{
    std::variant<const char*, double, int> value =
        valueFromColumn(column, item);

    if (std::holds_alternative<int>(value)) {
        ImGui::Text(format, std::get<int>(value));
    }
    else if (std::holds_alternative<double>(value)) {
        double v = std::get<double>(value);
        if (std::isnan(v)) {
            ImGui::TextUnformatted("");
        }
        else {
            ImGui::Text(format, std::get<double>(value));
        }
    }
    else if (std::holds_alternative<const char*>(value)) {
        ImGui::Text(format, std::get<const char*>(value));
    }
}

bool DataViewer::compareColumnValues(ColumnID column, const ExoplanetItem& left,
                                     const ExoplanetItem& right)
{
    std::variant<const char*, double, int> leftValue =
        valueFromColumn(column, left);

    std::variant<const char*, double, int> rightValue =
        valueFromColumn(column, right);

    // TODO: make sure they are the same type

    if (std::holds_alternative<const char*>(leftValue)) {
        return !caseInsensitiveLessThan(
            std::get<const char*>(leftValue),
            std::get<const char*>(rightValue)
        );
    }
    else if (std::holds_alternative<double>(leftValue)) {
        return compareValues(std::get<double>(leftValue), std::get<double>(rightValue));
    }
    else if (std::holds_alternative<int>(leftValue)) {
        return std::get<int>(leftValue) < std::get<int>(rightValue);
    }
    else {
        LERROR("Trying to compare undefined column types");
        return false;
    }
}

std::variant<const char*, double, int> DataViewer::valueFromColumn(ColumnID column,
                                                               const ExoplanetItem& item)
{
    switch (column) {
    case ColumnID::Name:
        return item.planetName.c_str();
    case ColumnID::Host:
        return item.hostName.c_str();
    case ColumnID::DiscoveryYear:
        return item.discoveryYear;
    case ColumnID::NPlanets:
        return item.nPlanets;
    case ColumnID::NStars:
        return item.nStars;
    case ColumnID::ESM:
        return item.esm;
    case ColumnID::TSM:
        return item.tsm;
    case ColumnID::PlanetRadius:
        return item.radius.value;
    case ColumnID::PlanetTemperature:
        return item.eqilibriumTemp.value;
    case ColumnID::PlanetMass:
        return item.mass.value;
    case ColumnID::SurfaceGravity:
        return item.surfaceGravity.value;
    // Orbits
    case ColumnID::SemiMajorAxis:
        return item.semiMajorAxis.value;
    case ColumnID::Eccentricity:
        return item.eccentricity.value;
    case ColumnID::Period:
        return item.period.value;
    case ColumnID::Inclination:
        return item.inclination.value;
    // Star
    case ColumnID::StarTemperature:
        return item.starEffectiveTemp.value;
    case ColumnID::StarRadius:
        return item.starRadius.value;
    case ColumnID::MagnitudeJ:
        return item.magnitudeJ.value;
    case ColumnID::MagnitudeK:
        return item.magnitudeK.value;
    case ColumnID::Distance:
        return item.distance.value;
    default:
        LERROR("Undefined column");
        return "undefined";
    }
}

} // namespace openspace::gui

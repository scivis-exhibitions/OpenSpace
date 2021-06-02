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

#include <implot.h>

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
    , _pointsIdentifier("ExoplanetDataPoints")
{
    _data = _dataLoader.loadData();

    _tableData.reserve(_data.size());
    _filteredData.reserve(_data.size());
    _positions.reserve(_data.size());

    int counter = 0;
    for (size_t i = 0; i < _data.size(); i++) {
        TableItem item;
        item.index = i;

        if (_data[i].position.has_value()) {
            _positions.push_back(_data[i].position.value());
            item.positionIndex = counter;
            counter++;
        }
        _tableData.push_back(item);

        _filteredData.push_back(i);
    }
    _positions.shrink_to_fit();
}

void DataViewer::initialize() {
    initializeRenderables();
}

void DataViewer::initializeRenderables() {
    using namespace std::string_literals;

    ghoul::Dictionary positions;
    int counter = 1;
    for (int i = 0; i < _positions.size(); ++i) {
        std::string index = fmt::format("[{}]", i + 1);
        positions.setValue<glm::dvec3>(index, _positions[i]);
    }

    ghoul::Dictionary gui;
    gui.setValue("Name", "All Exoplanets"s);
    gui.setValue("Path", "/ExoplanetsTool"s);

    ghoul::Dictionary renderable;
    renderable.setValue("Type", "RenderablePointData"s);
    renderable.setValue("Color", glm::dvec3(0.9, 1.0, 0.5));
    renderable.setValue("HighlightColor", glm::dvec3(0.0, 1.0, 0.8));
    renderable.setValue("Size", 10.0);
    renderable.setValue("Positions", positions);

    ghoul::Dictionary node;
    node.setValue("Identifier", _pointsIdentifier);
    node.setValue("Renderable", renderable);
    node.setValue("GUI", gui);

    openspace::global::scriptEngine->queueScript(
        fmt::format("openspace.addSceneGraphNode({})", ghoul::formatLua(node)),
        scripting::ScriptEngine::RemoteScripting::Yes
    );
}

void DataViewer::render() {
    renderTable();
    renderScatterPlot();
}

void DataViewer::renderScatterPlot() {
    int nPoints = static_cast<int>(_filteredData.size());

    static const ImVec2 size = { 400, 300 };
    auto plotFlags = ImPlotFlags_NoLegend;
    auto axisFlags = ImPlotAxisFlags_None;

    // TODO: Currently there are artifacts when rendering over 3200-ish points.
    // This fixed limit should be removed once I have figured out the rendering problems, or at
    // least only render the stars (which are about 3000)
    static const int maxPoints = 3225;
    if (nPoints > maxPoints) {
        nPoints = maxPoints;
    }

    // TODO: make static varaibles and only update data if filter and/or selection changed

    std::vector<double> ra, dec;
    ra.reserve(_filteredData.size());
    dec.reserve(_filteredData.size());

    for (int i : _filteredData) {
        const ExoplanetItem& item = _data[i];
        if (item.ra.hasValue() && item.ra.hasValue()) {
            ra.push_back(item.ra.value);
            dec.push_back(item.dec.value);
        }
    }

    std::vector<double> ra_selected, dec_selected;
    ra_selected.reserve(_selection.size());
    dec_selected.reserve(_selection.size());

    for (int i : _selection) {
        const ExoplanetItem& item = _data[i];
        if (item.ra.hasValue() && item.ra.hasValue()) {
            ra_selected.push_back(item.ra.value);
            dec_selected.push_back(item.dec.value);
        }
    }

    ImPlot::SetNextPlotLimits(0.0, 360.0, -90.0, 90.0);
    if (ImPlot::BeginPlot("Star Coordinate", "Ra", "Dec", size, plotFlags, axisFlags)) {
        ImPlot::PushStyleVar(ImPlotStyleVar_MarkerSize, 2);
        ImPlot::PlotScatter("Data", ra.data(), dec.data(), nPoints);
        ImPlot::PushStyleVar(ImPlotStyleVar_MarkerSize, 4);
        ImPlot::PlotScatter("Selected", ra_selected.data(), dec_selected.data(), ra_selected.size());
        ImPlot::EndPlot();
    }

    // TEST: to illustrate rendering issues
    srand(0);
    static const int nTestPoints = 5000;
    static int nPointsToRender = 5000;
    ImGui::InputInt("nPoints", &nPointsToRender);

    static float randRa[nTestPoints], randDec[nTestPoints];
    for (int i = 0; i < nTestPoints; ++i) {
        randRa[i] = 360.f * ((float)rand() / (float)RAND_MAX);
        randDec[i] = -90.f + 180.f * ((float)rand() / (float)RAND_MAX);
    }

    if (nPointsToRender > nTestPoints) {
        nPointsToRender = nTestPoints;
    }

    ImPlot::SetNextPlotLimits(0.0, 360.0, -90.0, 90.0);
    ImPlot::PushStyleVar(ImPlotStyleVar_MarkerSize, 2);
    if (ImPlot::BeginPlot("Random Ra Dec", "Ra", "Dec", size, plotFlags, axisFlags)) {
        ImPlot::PlotScatter("Test", randRa, randDec, nPointsToRender);
        ImPlot::EndPlot();
    }
}

void DataViewer::renderTable() {
    static const ImVec2 size = { 0, 400 };

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

    static bool hideNanTsm = false;
    static bool hideNanEsm = false;
    static bool showOnlyMultiPlanetSystems = false;

    // Filtering
    filterChanged |= ImGui::Checkbox("Hide nan TSM", &hideNanTsm);
    ImGui::SameLine();
    filterChanged |= ImGui::Checkbox("Hide nan ESM", &hideNanEsm);
    ImGui::SameLine();
    filterChanged |= ImGui::Checkbox("Only multi-planet", &showOnlyMultiPlanetSystems);

    static ImGuiTextFilter filter;
    filterChanged |= filter.Draw();

    ImGui::SameLine();
    if (ImGui::Button("Clear")) {
        filter.Clear();
        filterChanged = true;
    }

    if (filterChanged) {
        _filteredData.clear();
        _filteredData.reserve(_tableData.size());

        for (TableItem& f : _tableData) {
            const ExoplanetItem& d = _data[f.index];

            // TODO: implement filter per column

            bool filteredOut = hideNanTsm && std::isnan(d.tsm);
            filteredOut |= hideNanEsm && std::isnan(d.esm);
            filteredOut |= showOnlyMultiPlanetSystems && !d.multiSystemFlag;
            filteredOut |= !(filter.PassFilter(d.planetName.c_str()));

            if (!filteredOut) {
                _filteredData.push_back(f.index);
            }

            // If a filteredOut item is selected, remove it from selection
            if (filteredOut) {
                auto found = std::find(_selection.begin(), _selection.end(), f.index);
                const bool itemIsSelected = found != _selection.end();

                if (itemIsSelected) {
                    _selection.erase(found);
                    selectionChanged = true;
                }
            }
        }
        _filteredData.shrink_to_fit();
    }

    if (ImGui::BeginTable("exoplanets_table", nColumns, flags, size)) {
        // Header
        for (auto c : columns) {
            auto colFlags = c.flags | ImGuiTableColumnFlags_PreferSortDescending;
            ImGui::TableSetupColumn(c.name, colFlags, 0.f, c.id);
        }
        ImGui::TableSetupScrollFreeze(0, 1); // Make header always visible
        ImGui::TableHeadersRow();

        // Sorting
        if (ImGuiTableSortSpecs* sortSpecs = ImGui::TableGetSortSpecs()) {
            if (sortSpecs->SpecsDirty || filterChanged) {
                auto compare = [&sortSpecs, this](const size_t& lhs,
                                                  const size_t& rhs) -> bool
                {
                    ImGuiSortDirection sortDir = sortSpecs->Specs->SortDirection;
                    bool flip = (sortDir == ImGuiSortDirection_Descending);

                    const ExoplanetItem& l = flip ? _data[rhs] : _data[lhs];
                    const ExoplanetItem& r = flip ? _data[lhs] : _data[rhs];

                    ColumnID col = static_cast<ColumnID>(sortSpecs->Specs->ColumnUserID);

                    return compareColumnValues(col, l, r);
                };

                std::sort(_filteredData.begin(), _filteredData.end(), compare);
                sortSpecs->SpecsDirty = false;
            }
        }

        // Rows
        for (size_t row = 0; row < _filteredData.size(); row++) {
            if (row > 1000) {
                // TODO: show a hint about the number of rendered rows somewhere in the UI
                break; // cap the maximum number of rows we render
            }

            const size_t index = _filteredData[row];
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

        if (filterChanged) {
            const std::string positionIndices = composePositionIndexList(_filteredData);
            const std::string uri =
                fmt::format("Scene.{}.Renderable.Filtered", _pointsIdentifier);

            openspace::global::scriptEngine->queueScript(
                "openspace.setPropertyValueSingle('" + uri + "', { " + positionIndices + " })",
                scripting::ScriptEngine::RemoteScripting::Yes
            );
        }

        // Update selection renderable
        if (selectionChanged) {
            const std::string positionIndices = composePositionIndexList(_selection);
            const std::string uri =
                fmt::format("Scene.{}.Renderable.Selection", _pointsIdentifier);

            openspace::global::scriptEngine->queueScript(
                "openspace.setPropertyValueSingle('" + uri + "', { " + positionIndices + " })",
                scripting::ScriptEngine::RemoteScripting::Yes
            );
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

std::string DataViewer::composePositionIndexList(const std::vector<size_t>& dataIndices)
{
    std::string result;
    for (size_t s : dataIndices) {
        if (_tableData[s].positionIndex.has_value()) {
            const size_t posIndex = _tableData[s].positionIndex.value();
            result += std::to_string(posIndex) + ',';
        }
    }
    if (!result.empty()) {
        result.pop_back();
    }
    return result;
}

} // namespace openspace::gui

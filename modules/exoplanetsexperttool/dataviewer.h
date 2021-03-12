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

#ifndef __OPENSPACE_MODULE_EXOPLANETSEXPERTTOOL___DATAVIEWER___H__
#define __OPENSPACE_MODULE_EXOPLANETSEXPERTTOOL___DATAVIEWER___H__

#include <openspace/properties/propertyowner.h>

#include <modules/exoplanetsexperttool/dataloader.h>
#include <modules/exoplanetsexperttool/datastructures.h>
#include <vector>

namespace openspace::exoplanets::gui {

// @TODO give it a less generic name
class DataViewer : public properties::PropertyOwner {
public:
    DataViewer(std::string identifier, std::string guiName = "");

    void initialize();
    void render();

private:
    void initializeRenderables();
    void renderTable();

    DataLoader _dataLoader;
    std::vector<ExoplanetItem> _data;

    struct TableItem {
        int index;
        bool isVisible = true;
    };
    std::vector<TableItem> _tableData;

    std::string _allPointsIdentifier;
    std::string _selectedPointsIdentifier;

    bool _hideNanTsm = false;
    bool _hideNanEsm = false;
    bool _showOnlyMultiPlanetSystems = false;
};

} // namespace openspace::exoplanets::gui

#endif // __OPENSPACE_MODULE_EXOPLANETSEXPERTTOOL___DATAVIEWER___H__

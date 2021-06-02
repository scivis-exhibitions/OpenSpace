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

#ifndef __OPENSPACE_MODULE_EXOPLANETSEXPERTTOOL___RENDERABLEPOINTDATA___H__
#define __OPENSPACE_MODULE_EXOPLANETSEXPERTTOOL___RENDERABLEPOINTDATA___H__

#include <openspace/rendering/renderable.h>

#include <openspace/properties/list/intlistproperty.h>
#include <openspace/properties/scalar/floatproperty.h>
#include <openspace/properties/vector/vec3property.h>
#include <ghoul/opengl/ghoul_gl.h>
#include <ghoul/opengl/uniformcache.h>

namespace openspace::documentation { struct Documentation; }

namespace ghoul::filesystem { class File; }
namespace ghoul::opengl { class ProgramObject; }

namespace openspace {

class RenderablePointData : public Renderable {
public:
    RenderablePointData(const ghoul::Dictionary& dictionary);

    void initializeGL() override;
    void deinitializeGL() override;

    bool isReady() const override;

    void render(const RenderData& data, RendererTasks& rendererTask) override;
    void update(const UpdateData& data) override;

    /**
     * Update the points data in this renderable
     * positions \in Parsec
     */
    void initializeData(const std::vector<glm::dvec3> positions);

    static documentation::Documentation Documentation();

private:
    bool _isDirty = true;
    bool _selectionChanged = true;

    std::unique_ptr<ghoul::opengl::ProgramObject> _shaderProgram = nullptr;
    UniformCache(modelViewTransform, modelViewProjectionTransform,
        color, opacity, size) _uniformCache;

    properties::Vec3Property _color;
    properties::Vec3Property _highlightColor;
    properties::FloatProperty _size;
    properties::FloatProperty _selectedSizeScale;
    properties::IntListProperty _selectedIndices;

    struct Point {
        float xyz[3];
    };
    const unsigned int _nValuesPerPoint = 3;

    std::vector<Point> _pointData;

    GLuint _primaryPointsVAO = 0;
    GLuint _primaryPointsVBO = 0;

    GLuint _selectedPointsVAO = 0;
    GLuint _selectedPointsVBO = 0;
};

}// namespace openspace

#endif // __OPENSPACE_MODULE_EXOPLANETSEXPERTTOOL___RENDERABLEPOINTDATA___H__

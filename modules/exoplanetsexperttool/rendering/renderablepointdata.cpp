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

#include <modules/exoplanetsexperttool/rendering/renderablepointdata.h>

#include <openspace/documentation/verifier.h>
#include <openspace/engine/globals.h>
#include <openspace/rendering/renderengine.h>
#include <openspace/util/distanceconstants.h>
#include <openspace/util/updatestructures.h>
#include <ghoul/filesystem/filesystem.h>
#include <ghoul/logging/logmanager.h>
#include <ghoul/opengl/openglstatecache.h>
#include <ghoul/opengl/programobject.h>
#include <fstream>
#include <optional>

namespace {
    constexpr const char* _loggerCat = "PointsCloud";

    constexpr const std::array<const char*, 5> UniformNames = {
        "modelViewTransform", "MVPTransform", "color", "opacity", "size",
    };

    constexpr openspace::properties::Property::PropertyInfo ColorInfo = {
        "Color",
        "Color",
        "The color of the points."
    };

    constexpr openspace::properties::Property::PropertyInfo SizeInfo = {
        "Size",
        "Size",
        "The size of the points."
    };

    constexpr openspace::properties::Property::PropertyInfo PositionsInfo = {
        "Positions",
        "Positions",
        "Data to use for the positions of the points, given in Parsec."
    };

    struct [[codegen::Dictionary(RenderablePointData)]] Parameters {
        // [[codegen::verbatim(ColorInfo.description)]]
        std::optional<glm::vec3> color [[codegen::color()]];

        // [[codegen::verbatim(SizeInfo.description)]]
        std::optional<float> size;

        // [[codegen::verbatim(PositionsInfo.description)]]
        std::vector<glm::dvec3> positions;
    };
#include "renderablepointdata_codegen.cpp"
} // namespace

namespace openspace {

documentation::Documentation RenderablePointData::Documentation() {
    return codegen::doc<Parameters>("exoplanetsexperttool_renderable_pointdata");
}

RenderablePointData::RenderablePointData(const ghoul::Dictionary& dictionary)
    : Renderable(dictionary)
    , _color(ColorInfo, glm::vec3(0.5f), glm::vec3(0.f), glm::vec3(1.f))
    , _size(SizeInfo, 1.f, 0.f, 150.f)
{
    const Parameters p = codegen::bake<Parameters>(dictionary);

    _color = p.color.value_or(_color);
    _color.setViewOption(properties::Property::ViewOptions::Color);
    addProperty(_color);

    updateData(p.positions);

    _size = p.size.value_or(_size);
    addProperty(_size);

    addProperty(_opacity);
}

bool RenderablePointData::isReady() const {
    return _shaderProgram != nullptr;
}

void RenderablePointData::initializeGL() {
    _shaderProgram = global::renderEngine->buildRenderProgram(
        "ExoPointsCloud",
        absPath("${MODULE_EXOPLANETSEXPERTTOOL}/shaders/points_vs.glsl"),
        absPath("${MODULE_EXOPLANETSEXPERTTOOL}/shaders/points_fs.glsl")
    );

    ghoul::opengl::updateUniformLocations(*_shaderProgram, _uniformCache, UniformNames);
}

void RenderablePointData::deinitializeGL() {
    glDeleteVertexArrays(1, &_vertexArrayObjectID);
    _vertexArrayObjectID = 0;

    glDeleteBuffers(1, &_vertexBufferObjectID);
    _vertexBufferObjectID = 0;

    if (_shaderProgram) {
        global::renderEngine->removeRenderProgram(_shaderProgram.get());
        _shaderProgram = nullptr;
    }
}

void RenderablePointData::render(const RenderData& data, RendererTasks&) {
    if (_pointData.empty()) {
        return;
    }

    _shaderProgram->activate();

    glm::dmat4 modelTransform =
        glm::translate(glm::dmat4(1.0), data.modelTransform.translation) * // Translation
        glm::dmat4(data.modelTransform.rotation) *  // Spice rotation
        glm::scale(glm::dmat4(1.0), glm::dvec3(data.modelTransform.scale));

    glm::dmat4 modelViewTransform = data.camera.combinedViewMatrix() * modelTransform;

    _shaderProgram->setUniform(_uniformCache.modelViewTransform, modelViewTransform);
    _shaderProgram->setUniform(
        _uniformCache.modelViewProjectionTransform,
        glm::dmat4(data.camera.projectionMatrix()) * modelViewTransform
    );

    _shaderProgram->setUniform(_uniformCache.color, _color);
    _shaderProgram->setUniform(_uniformCache.opacity, _opacity);
    _shaderProgram->setUniform(_uniformCache.size, _size);

    // Changes GL state:
    glEnablei(GL_BLEND, 0);
    glDepthMask(false);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_PROGRAM_POINT_SIZE); // Enable gl_PointSize in vertex

    glBindVertexArray(_vertexArrayObjectID);
    const GLsizei nPoints = static_cast<GLsizei>(_pointData.size() / 3);
    glDrawArrays(GL_POINTS, 0, nPoints);

    glBindVertexArray(0);
    _shaderProgram->deactivate();

    // Restores GL State
    global::renderEngine->openglStateCache().resetBlendState();
    global::renderEngine->openglStateCache().resetDepthState();
}

void RenderablePointData::update(const UpdateData&) {
    if (_shaderProgram->isDirty()) {
        _shaderProgram->rebuildFromFile();
        ghoul::opengl::updateUniformLocations(*_shaderProgram, _uniformCache, UniformNames);
    }

    if (!_isDirty) {
        return;
    }

    if (_vertexArrayObjectID == 0) {
        glGenVertexArrays(1, &_vertexArrayObjectID);
        LDEBUG(fmt::format("Generating Vertex Array id '{}'", _vertexArrayObjectID));
    }
    if (_vertexBufferObjectID == 0) {
        glGenBuffers(1, &_vertexBufferObjectID);
        LDEBUG(fmt::format("Generating Vertex Buffer Object id '{}'", _vertexBufferObjectID));
    }

    glBindVertexArray(_vertexArrayObjectID);
    glBindBuffer(GL_ARRAY_BUFFER, _vertexBufferObjectID);
    glBufferData(
        GL_ARRAY_BUFFER,
        _pointData.size() * sizeof(float),
        _pointData.data(),
        GL_STATIC_DRAW
    );

    GLint positionAttribute = _shaderProgram->attributeLocation("in_position");

    glEnableVertexAttribArray(positionAttribute);
    glVertexAttribPointer(
        positionAttribute,
        3,
        GL_FLOAT,
        GL_FALSE,
        0,
        nullptr
    );

    glBindVertexArray(0);

    _isDirty = false;
}

void RenderablePointData::updateData(const std::vector<glm::dvec3> positions) {
    _pointData.clear();
    _pointData.reserve(3 * positions.size());
    for (const glm::dvec3& pos : positions) {
        constexpr double Parsec = distanceconstants::Parsec;
        _pointData.push_back(pos.x * Parsec);
        _pointData.push_back(pos.y * Parsec);
        _pointData.push_back(pos.z * Parsec);
    }

    _isDirty = true;
}

} // namespace openspace

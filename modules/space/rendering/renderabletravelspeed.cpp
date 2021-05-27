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
#include <modules/space/rendering/renderabletravelspeed.h>

#include <modules/base/basemodule.h>
#include <openspace/documentation/documentation.h>
#include <openspace/engine/globals.h>
#include <openspace/query/query.h>
#include <openspace/rendering/renderengine.h>
#include <openspace/scene/scenegraphnode.h>
#include <openspace/util/distanceconstants.h>
#include <openspace/util/timemanager.h>
#include <openspace/util/updatestructures.h>
#include <ghoul/filesystem/filesystem.h>

#include <optional>

namespace {
    constexpr const char* _loggerCat = "renderableTravelSpeed";

    constexpr const std::array<const char*, 2> UniformNames = {
    "lineColor", "opacity"
    };

    constexpr openspace::properties::Property::PropertyInfo SpeedInfo = {
    "travelSpeed",
    "Speed of travel",
    "The speed of light is the default value."
    };

    constexpr openspace::properties::Property::PropertyInfo TargetInfo = {
    "targetNode",
    "Target object",
    "This value sets which scene graph node to target with the light speed indicator"
    };

    constexpr openspace::properties::Property::PropertyInfo LineColorInfo = {
    "Color",
    "Color",
    "This value determines the RGB color for the line."
    };

    constexpr openspace::properties::Property::PropertyInfo LineOpacityInfo = {
    "Opacity",
    "Opacity",
    "This value determines the opacity for the line."
    };

    constexpr openspace::properties::Property::PropertyInfo LineWidthInfo = {
    "LineWidth",
    "Line Width",
    "This value specifies the line width."
    };

    constexpr openspace::properties::Property::PropertyInfo IndicatorLengthInfo = {
    "IndicatorLength",
    "Indicator Length",
    "This value specifies the length of the light indicator set in light seconds."
    };

    constexpr openspace::properties::Property::PropertyInfo FadeLengthInfo = {
    "FadeLength",
    "Fade Length",
    "This value specifies the length of the faded tail of the light indicator "
    "set in light seconds."
    };

    struct [[codegen::Dictionary(RenderableLightTravel)]] Parameters {
        // [[codegen::verbatim(TargetInfo.description)]]
        std::string target;

        // [[codegen::verbatim(SpeedInfo.description)]]
        std::optional<double> travelSpeed;

        // [[codegen::verbatim(LineColorInfo.description)]]
        std::optional<glm::vec3> color;

        // [[codegen::verbatim(LineOpacityInfo.description)]]
        std::optional<float> opacity;

        // [[codegen::verbatim(LineWidthInfo.description)]]
        std::optional<float> lineWidth;
        
        // [[codegen::verbatim(IndicatorLengthInfo.description)]]
        std::optional<int> indicatorLength;
        
        // [[codegen::verbatim(FadeLengthInfo.description)]]
        std::optional<int> fadeLength;

        
    };
#include "renderabletravelspeed_codegen.cpp"
} // namespace

namespace openspace {
    using namespace properties;

documentation::Documentation RenderableTravelSpeed::Documentation() {
    return codegen::doc<Parameters>("base_renderable_renderabletravelspeed");
}

RenderableTravelSpeed::RenderableTravelSpeed(const ghoul::Dictionary& dictionary)
    : Renderable(dictionary)
    , _targetName(TargetInfo)
    , _travelSpeed(SpeedInfo, distanceconstants::LightSecond, 1.0, 
        distanceconstants::LightSecond)
    , _indicatorLength(IndicatorLengthInfo, 1, 1, 360)
    , _fadeLength(FadeLengthInfo, 1, 0, 360)
    , _lineColor(LineColorInfo, glm::vec3(1.f), glm::vec3(0.f), glm::vec3(1.f))
    , _opacity(LineOpacityInfo, 1.0, 0.0, 1.0)
    , _lineWidth(LineWidthInfo, 2.f, 1.f, 20.f)

{
    const Parameters p = codegen::bake<Parameters>(dictionary);
    setRenderBin(RenderBin::Overlay);

    _lineColor = p.color.value_or(_lineColor);
    addProperty(_lineColor);

    _opacity = p.opacity.value_or(_opacity);
    addProperty(_opacity);

    _lineWidth = p.lineWidth.value_or(_lineWidth);
    addProperty(_lineWidth);

    _indicatorLength = p.indicatorLength.value_or(_indicatorLength);
    addProperty(_indicatorLength);

    _fadeLength = p.fadeLength.value_or(_fadeLength);
    addProperty(_fadeLength);

    _targetName = p.target;
    addProperty(_targetName);
    _targetName.onChange([this]() {
        if (SceneGraphNode* n = sceneGraphNode(_targetName); n) {
            _targetNode = n;
            _targetPosition = _targetNode->worldPosition();
            _lightTravelTime = calculateLightTravelTime(
                _sourcePosition, _targetPosition
            );
            calculateDirectionVector();
            reinitiateTravel();
        }
    });

    _travelSpeed = p.travelSpeed.value_or(_travelSpeed);
    addProperty(_travelSpeed);
    _travelSpeed.onChange([this]() {
        reinitiateTravel();
    });

} // constructor

void RenderableTravelSpeed::initialize() {

    _initiationTime = -1;
    _targetNode = sceneGraphNode(_targetName);

} // initialize

void RenderableTravelSpeed::initializeGL() {

    _shaderProgram = BaseModule::ProgramObjectManager.request(
        "Travelspeed",
        []() -> std::unique_ptr<ghoul::opengl::ProgramObject> {
            return global::renderEngine->buildRenderProgram(
                "Travelspeed",
                absPath("${MODULE_SPACE}/shaders/travelspeed_vs.glsl"),
                absPath("${MODULE_SPACE}/shaders/travelspeed_fs.glsl")
            );
        }
    );

    glGenVertexArrays(1, &_vaoId);
    glGenBuffers(1, &_vBufferId);

    ghoul::opengl::updateUniformLocations(*_shaderProgram, _uniformCache, UniformNames);

} // initializeGL

void RenderableTravelSpeed::deinitializeGL() {
    BaseModule::ProgramObjectManager.release(
        "Travelspeed",
        [](ghoul::opengl::ProgramObject* p) {
            global::renderEngine->removeRenderProgram(p);
        }
    );
    glDeleteVertexArrays(1, &_vaoId);
    glDeleteBuffers(1, &_vBufferId);
} // deinitializeGL

double RenderableTravelSpeed::calculateLightTravelTime(glm::dvec3 startPosition,
    glm::dvec3 targetPosition) {
    return glm::distance(targetPosition, startPosition) /
        _travelSpeed;
}

void RenderableTravelSpeed::calculateDirectionVector() {
    _directionVector = glm::normalize(_targetPosition - _sourcePosition);
}

void RenderableTravelSpeed::calculateVerticesPositions() {

    // 3: start of light, 2: start of fade, 1: end of fade
    _verticesPositions[2].x = _travelSpeed * _timeSinceStart * _directionVector.x;
    _verticesPositions[2].y = _travelSpeed * _timeSinceStart * _directionVector.y;
    _verticesPositions[2].z = _travelSpeed * _timeSinceStart * _directionVector.z;

    // This if statment is there to not start the line from behind the source node
    if (_timeSinceStart < _indicatorLength) {
        _verticesPositions[1] = glm::vec3(0.0, 0.0, 0.0); // = source node
    }
    else {
        _verticesPositions[1].x =
            _travelSpeed * _directionVector.x * (_timeSinceStart - _indicatorLength);        
        _verticesPositions[1].y =
            _travelSpeed * _directionVector.y * (_timeSinceStart - _indicatorLength);
        _verticesPositions[1].z =
            _travelSpeed * _directionVector.z * (_timeSinceStart - _indicatorLength);

    }

    // This if statment is there to not start the line from behind the source node
    if (_timeSinceStart < (_indicatorLength + _fadeLength)) {
        _verticesPositions[0] = glm::vec3(0.0, 0.0, 0.0); // = source node
    }
    else {
        _verticesPositions[0].x =
            _travelSpeed * _directionVector.x * 
            (_timeSinceStart - _indicatorLength - _fadeLength);
        _verticesPositions[0].y =
            _travelSpeed * _directionVector.y * 
            (_timeSinceStart - _indicatorLength - _fadeLength);
        _verticesPositions[0].z =
            _travelSpeed * _directionVector.z * 
            (_timeSinceStart - _indicatorLength - _fadeLength);
    }
}

void RenderableTravelSpeed::updateVertexData() {

    calculateVerticesPositions();

    glBindVertexArray(_vaoId);
    glBindBuffer(GL_ARRAY_BUFFER, _vBufferId);
    glBufferData(
        GL_ARRAY_BUFFER,
        sizeof(_verticesPositions),
        _verticesPositions,
        GL_DYNAMIC_DRAW
    );
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3*sizeof(float), nullptr);
    glBindVertexArray(0);
}

void RenderableTravelSpeed::reinitiateTravel() {
    _initiationTime = global::timeManager->time().j2000Seconds();
    _arrivalTime = _initiationTime + _lightTravelTime;
}

bool RenderableTravelSpeed::isReady() const{
    return _shaderProgram != nullptr;
}

void RenderableTravelSpeed::update(const UpdateData& data) {
    
    if (_initiationTime == -1) {
        _initiationTime = data.time.j2000Seconds();
        std::string_view temp = data.time.ISO8601();
        _arrivalTime = _initiationTime + _lightTravelTime;
    }

    _targetPosition = _targetNode->worldPosition();
    SceneGraphNode* mySGNPointer = dynamic_cast<SceneGraphNode*>(owner());
    ghoul_assert(mySGNPointer, "Renderable have to be owned by scene graph node");
    _sourcePosition = mySGNPointer->worldPosition();

    _lightTravelTime = calculateLightTravelTime(
        _sourcePosition, _targetPosition
    );

    const double currentTime = data.time.j2000Seconds();
    // Unless we've reached the target
    if (_initiationTime < currentTime && _arrivalTime > currentTime) {

        _timeSinceStart = currentTime - _initiationTime;
        calculateDirectionVector();
        updateVertexData();

    }
    else { // in case we've reached the target
        reinitiateTravel();
    }

    _shaderProgram->setUniform("lineColor", _lineColor);
    _shaderProgram->setUniform("opacity", _opacity);

} // update

void RenderableTravelSpeed::render(const RenderData& data, RendererTasks& ) {
    if (!this->_enabled) return;

    _shaderProgram->activate();

    const glm::dmat4 modelTransform =
        glm::translate(glm::dmat4(1.0), data.modelTransform.translation) *
        glm::dmat4(data.modelTransform.rotation) *
        glm::scale(glm::dmat4(1.0), glm::dvec3(data.modelTransform.scale));

    const glm::dmat4 modelViewTransform = data.camera.combinedViewMatrix() *
        modelTransform;

    _shaderProgram->setUniform("modelViewTransform", glm::mat4(modelViewTransform));
    _shaderProgram->setUniform("projectionTransform", data.camera.projectionMatrix());

    glLineWidth(_lineWidth);
#ifdef __APPLE__
    glLineWidth(1);
#endif
    glBindVertexArray(_vaoId);
    glBindBuffer(GL_ARRAY_BUFFER, _vBufferId);
    glDrawArrays(
        GL_LINE_STRIP,
        0,
        3 
    );
    glBindVertexArray(0);

    _shaderProgram->deactivate();

} // render
} // namespace
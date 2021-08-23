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

#ifndef __OPENSPACE_CORE___EVENT___H__
#define __OPENSPACE_CORE___EVENT___H__

#include <ghoul/misc/assert.h>

namespace openspace {
    namespace properties { class Property; }

    class Camera;
    class Layer;
    class Profile;
    class SceneGraphNode;
    class ScreenSpaceRenderable;
    class Time;
} // namepsace opensppace

namespace openspace::events {

struct Event {
    enum class Type {
        SceneGraphNodeAdded,
        SceneGraphNodeRemoved,
        PropertyAdded,
        PropertyRemoved,
        ParallelConnectionEstablished,
        ParallelConnectionLost,
        ParallelConnectionHostshipGained,
        ParallelConnectionHostshipLost,
        ProfileLoadingFinished,
        ApplicationShutdownStarted,
        ScreenSpaceRenderableAdded,
        ScreenSpaceRenderableRemoved,
        CameraApproachedSceneGraphNode,
        CameraMovedAwayFromSceneGraphNode,
        TimeOfInterestReached,
        MissionEventReached,
        PlanetEclipsed,
        Custom
    };
    Event(Type type_) : type(type_) {}

    const Type type;
    const Event* next = nullptr;
};

template <typename T>
T* asType(Event* e) {
    ghoul_assert(e->type == T::Type, "Wrong type requested, check 'isType'");
    return static_cast<T*>(e);
}

template <typename T>
bool isType(Event* e) {
    return e->type == T::Type;
}

void logAllEvents(const Event* e);

//
//  Events
//

struct EventSceneGraphNodeAdded : public Event {
    static const Type Type = Event::Type::SceneGraphNodeAdded;

    EventSceneGraphNodeAdded(const SceneGraphNode* node_);
    const SceneGraphNode* node = nullptr;
};


struct EventSceneGraphNodeRemoved : public Event {
    static const Type Type = Event::Type::SceneGraphNodeRemoved;
    
    EventSceneGraphNodeRemoved(const SceneGraphNode* node_);
    const SceneGraphNode* node = nullptr;
};


struct EventPropertyAdded : public Event {
    static const Type Type = Event::Type::PropertyAdded;

    EventPropertyAdded(const properties::Property* property_);
    const properties::Property* property = nullptr;
};


struct EventPropertyRemoved : public Event {
    static const Type Type = Event::Type::PropertyRemoved;

    EventPropertyRemoved(const properties::Property* property_);
    const properties::Property* property = nullptr;
};


struct EventParallelConnectionEstablished : public Event {
    static const Type Type = Event::Type::ParallelConnectionEstablished;

    EventParallelConnectionEstablished();
};


struct EventParallelConnectionLost : public Event {
    static const Type Type = Event::Type::ParallelConnectionLost;

    EventParallelConnectionLost();
};


struct EventParallelConnectionHostshipGained : public Event {
    static const Type Type = Event::Type::ParallelConnectionHostshipGained;

    EventParallelConnectionHostshipGained();
};


struct EventParallelConnectionHostshipLost : public Event {
    static const Type Type = Event::Type::ParallelConnectionHostshipLost;

    EventParallelConnectionHostshipLost();
};


struct EventProfileLoadingFinished : public Event {
    static const Type Type = Event::Type::ProfileLoadingFinished;

    EventProfileLoadingFinished(const Profile* profile_);
    const Profile* profile = nullptr;
};


struct EventApplicationShutdownStarted : public Event {
    static const Type Type = Event::Type::ApplicationShutdownStarted;

    EventApplicationShutdownStarted();
};


struct EventScreenSpaceRenderableAdded : public Event {
    static const Type Type = Event::Type::ScreenSpaceRenderableAdded;

    EventScreenSpaceRenderableAdded(const ScreenSpaceRenderable* renderable_);
    const ScreenSpaceRenderable* renderable = nullptr;
};


struct EventScreenSpaceRenderableRemoved : public Event {
    static const Type Type = Event::Type::ScreenSpaceRenderableRemoved;

    EventScreenSpaceRenderableRemoved(const ScreenSpaceRenderable* renderable_);

    const ScreenSpaceRenderable* renderable = nullptr;
};


struct EventCameraApproachedSceneGraphNode : public Event {
    static const Type Type = Event::Type::CameraApproachedSceneGraphNode;

    EventCameraApproachedSceneGraphNode(const Camera* camera_,
        const SceneGraphNode* node_);

    const Camera* camera = nullptr;
    const SceneGraphNode* node = nullptr;
};


struct EventCameraMovedAwayFromSceneGraphNode : public Event {
    static const Type Type = Event::Type::CameraMovedAwayFromSceneGraphNode;

    EventCameraMovedAwayFromSceneGraphNode(const Camera* camera_,
        const SceneGraphNode* node_);

    const Camera* camera = nullptr;
    const SceneGraphNode* node = nullptr;
};


struct EventTimeOfInterestReached : public Event {
    static const Type Type = Event::Type::TimeOfInterestReached;

    EventTimeOfInterestReached(const Time* time_, const Camera* camera_);
    const Time* time = nullptr;
    const Camera* camera = nullptr;
};


struct EventMissionEventReached : public Event {
    static const Type Type = Event::Type::MissionEventReached;

    // Not sure which kind of parameters we want to pass here
    EventMissionEventReached();
};


struct EventPlanetEclipsed : public Event {
    static const Type Type = Event::Type::PlanetEclipsed;

    EventPlanetEclipsed(const SceneGraphNode* eclipsee_, const SceneGraphNode* eclipser);
    const SceneGraphNode* eclipsee = nullptr;
    const SceneGraphNode* eclipser = nullptr;
};


struct CustomEvent : public Event {
    static const Type Type = Event::Type::Custom;

    CustomEvent(std::string_view subtype_, const void* payload_);

    const std::string_view subtype;
    const void* payload = nullptr;
};

} // namespace openspace::events

#endif // __OPENSPACE_CORE___EVENT___H__
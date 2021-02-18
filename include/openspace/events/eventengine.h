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

#ifndef __OPENSPACE_CORE___EVENTENGINE___H__
#define __OPENSPACE_CORE___EVENTENGINE___H__

#include <ghoul/misc/memorypool.h>

namespace openspace {

namespace events { struct Event; }

class EventEngine {
public:
    events::Event* firstEvent() const; // -> begin

    template <typename T, typename... Args>
    void publishEvent(Args&&... args);

    void postFrameCleanup();
    
private:
    //ghoul::MemoryPool<64> _memory;
    //ghoul::MemoryPool<4096> _memory;
    //ghoul::MemoryPool<20480> _memory;
    ghoul::MemoryPool<40960> _memory;
    events::Event* _firstEvent = nullptr;
    events::Event* _lastEvent = nullptr;

#ifdef _DEBUG
    static uint64_t nEvents;
#endif // _DEBUG
};

//
// Implementation
//

template <typename T, typename... Args>
void EventEngine::publishEvent(Args&&... args) {
    T* e = _memory.alloc<T>(args...);
    if (!_firstEvent) {
        _firstEvent = e;
        _lastEvent = e;
    }
    else {
        _lastEvent->next = e;
        _lastEvent = e;
    }

#ifdef _DEBUG
    nEvents++;
#endif // _DEBUG
}

} // namespace openspace

#endif // __OPENSPACE_CORE___EVENTENGINE___H__

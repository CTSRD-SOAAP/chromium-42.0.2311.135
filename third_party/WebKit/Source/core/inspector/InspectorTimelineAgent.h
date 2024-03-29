/*
* Copyright (C) 2012 Google Inc. All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are
* met:
*
*     * Redistributions of source code must retain the above copyright
* notice, this list of conditions and the following disclaimer.
*     * Redistributions in binary form must reproduce the above
* copyright notice, this list of conditions and the following disclaimer
* in the documentation and/or other materials provided with the
* distribution.
*     * Neither the name of Google Inc. nor the names of its
* contributors may be used to endorse or promote products derived from
* this software without specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
* "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
* LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
* A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
* OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
* SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
* LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
* DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
* THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
* OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef InspectorTimelineAgent_h
#define InspectorTimelineAgent_h

#include "core/inspector/InspectorBaseAgent.h"
#include "wtf/PassOwnPtr.h"

namespace blink {

typedef String ErrorString;

class InspectorTimelineAgent final
    : public InspectorBaseAgent<InspectorTimelineAgent>
    , public InspectorBackendDispatcher::TimelineCommandHandler {
    WTF_MAKE_NONCOPYABLE(InspectorTimelineAgent);
public:
    static PassOwnPtrWillBeRawPtr<InspectorTimelineAgent> create()
    {
        return adoptPtrWillBeNoop(new InspectorTimelineAgent());
    }

    virtual ~InspectorTimelineAgent();

    virtual void enable(ErrorString*) override;
    virtual void disable(ErrorString*) override;
    virtual void start(ErrorString*, const int* maxCallStackDepth, const bool* bufferEvents, const String* liveEvents, const bool* includeCounters, const bool* includeGPUEvents) override;
    virtual void stop(ErrorString*) override;

private:
    InspectorTimelineAgent();
};

} // namespace blink

#endif // !defined(InspectorTimelineAgent_h)

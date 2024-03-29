// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef StereoPanner_h
#define StereoPanner_h

#include "platform/audio/Spatializer.h"

namespace blink {

// Common type of stereo panner as found in normal audio mixing equipment.
// See: http://webaudio.github.io/web-audio-api/#the-stereopannernode-interface

class PLATFORM_EXPORT StereoPanner final : public Spatializer {
public:
    explicit StereoPanner(float sampleRate);

    virtual void panWithSampleAccurateValues(const AudioBus* inputBus, AudioBus* outputBuf, const float* panValues, size_t framesToProcess) override;
    virtual void panToTargetValue(const AudioBus* inputBus, AudioBus* outputBuf, float panValue, size_t framesToProcess) override;

    virtual void reset() override { }

    virtual double tailTime() const override { return 0; }
    virtual double latencyTime() const override { return 0; }

private:
    bool m_isFirstRender;
    double m_smoothingConstant;

    double m_gainL;
    double m_gainR;
};

} // namespace blink

#endif // StereoPanner_h

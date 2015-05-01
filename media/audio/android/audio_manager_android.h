// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_ANDROID_AUDIO_MANAGER_ANDROID_H_
#define MEDIA_AUDIO_ANDROID_AUDIO_MANAGER_ANDROID_H_

#include <set>

#include "base/android/jni_android.h"
#include "base/gtest_prod_util.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "media/audio/audio_manager_base.h"

namespace media {

class OpenSLESOutputStream;

// Android implemention of AudioManager.
class MEDIA_EXPORT AudioManagerAndroid : public AudioManagerBase {
 public:
  AudioManagerAndroid(AudioLogFactory* audio_log_factory);

  // Implementation of AudioManager.
  bool HasAudioOutputDevices() override;
  bool HasAudioInputDevices() override;
  void GetAudioInputDeviceNames(AudioDeviceNames* device_names) override;
  void GetAudioOutputDeviceNames(AudioDeviceNames* device_names) override;
  AudioParameters GetInputStreamParameters(
      const std::string& device_id) override;

  AudioOutputStream* MakeAudioOutputStream(
      const AudioParameters& params,
      const std::string& device_id) override;
  AudioInputStream* MakeAudioInputStream(const AudioParameters& params,
                                         const std::string& device_id) override;
  void ReleaseOutputStream(AudioOutputStream* stream) override;
  void ReleaseInputStream(AudioInputStream* stream) override;

  // Implementation of AudioManagerBase.
  AudioOutputStream* MakeLinearOutputStream(
      const AudioParameters& params) override;
  AudioOutputStream* MakeLowLatencyOutputStream(
      const AudioParameters& params,
      const std::string& device_id) override;
  AudioInputStream* MakeLinearInputStream(
      const AudioParameters& params,
      const std::string& device_id) override;
  AudioInputStream* MakeLowLatencyInputStream(
      const AudioParameters& params,
      const std::string& device_id) override;

  static bool RegisterAudioManager(JNIEnv* env);

  void SetMute(JNIEnv* env, jobject obj, jboolean muted);

 protected:
  ~AudioManagerAndroid() override;

  AudioParameters GetPreferredOutputStreamParameters(
      const std::string& output_device_id,
      const AudioParameters& input_params) override;

 private:
  void InitializeOnAudioThread();
  void ShutdownOnAudioThread();

  bool HasNoAudioInputStreams();
  void SetCommunicationAudioModeOn(bool on);
  bool SetAudioDevice(const std::string& device_id);
  int GetNativeOutputSampleRate();
  bool IsAudioLowLatencySupported();
  int GetAudioLowLatencyOutputFrameSize();
  int GetOptimalOutputFrameSize(int sample_rate, int channels);

  void DoSetMuteOnAudioThread(bool muted);

  // Java AudioManager instance.
  base::android::ScopedJavaGlobalRef<jobject> j_audio_manager_;

  typedef std::set<OpenSLESOutputStream*> OutputStreams;
  OutputStreams streams_;

  // Enabled when first input stream is created and set to false when last
  // input stream is destroyed. Also affects the stream type of output streams.
  bool communication_mode_is_on_;

  DISALLOW_COPY_AND_ASSIGN(AudioManagerAndroid);
};

}  // namespace media

#endif  // MEDIA_AUDIO_ANDROID_AUDIO_MANAGER_ANDROID_H_
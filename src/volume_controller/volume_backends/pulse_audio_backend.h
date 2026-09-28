#ifndef PULSE_AUDIO_BACKEND_H
#define PULSE_AUDIO_BACKEND_H

#include "volume_backend.h"
#include <condition_variable>
#include <iostream>
#include <memory>
#include <mutex>
#include <pulse/channelmap.h>
#include <pulse/context.h>
#include <pulse/mainloop.h>
#include <pulse/operation.h>
#include <pulse/pulseaudio.h>
#include <pulse/simple.h>
#include <pulse/volume.h>
#include <string>
#include <thread>

class PulseAudioBackend : public VolumeBackend {
  public:
    explicit PulseAudioBackend(
        const std::string &name = "PhonieBox++ PulseAudioBackend");
    ~PulseAudioBackend() override;

    bool Initialize() override;
    bool IsInitialized() const override;
    float ReadVolume() const override;
    void SetVolume(float volume) override;
    void Mute(bool enabled) override;

  private:
    static void sink_info_cb(pa_context *c, const pa_sink_info *i, int e,
                             void *user_data);
    static void context_state_cb(pa_context *c, void *user_data);
    static void mainloop_deleter(pa_mainloop *ml) {
        if (ml) pa_mainloop_free(ml);
    }
    static void context_deleter(pa_context *c) {
        if (c) pa_context_unref(c);
    }
    static void sink_info_deleter(pa_sink_info *info) {
        if (info) {
            std::cerr << "Warning: pa_sink_info_free is not available. "
                         "Using fallback cleanup."
                      << std::endl;
        }
    }

    std::unique_ptr<pa_mainloop, decltype(&mainloop_deleter)> mMainloop;
    std::unique_ptr<pa_context, decltype(&context_deleter)> mContext;
    std::unique_ptr<pa_sink_info, decltype(&sink_info_deleter)> mSinkInfo;
    std::thread mMainloopThread;
    mutable std::mutex mMutex;
    std::condition_variable mContextReady;
    bool mContextReadyFlag = false;
    bool mInitialized = false;
    std::string mName;
};

#endif // PULSE_AUDIO_BACKEND_H

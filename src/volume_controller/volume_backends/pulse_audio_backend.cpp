#include "pulse_audio_backend.h"
#include <chrono>
#include <pulse/channelmap.h>
#include <pulse/context.h>
#include <pulse/operation.h>
#include <pulse/proplist.h>
#include <pulse/volume.h>
#include <stdexcept>
#include <thread>

// Static callback for sink info
void PulseAudioBackend::sink_info_cb(pa_context *c, const pa_sink_info *i,
                                     int e, void *user_data) {
    if (e != PA_OK || !i) return;
    auto *backend = static_cast<PulseAudioBackend *>(user_data);
    std::lock_guard<std::mutex> lock(backend->mMutex);
    backend->mSinkInfo.reset(const_cast<pa_sink_info *>(i));
    backend->mInitialized = true;
    backend->mContextReady.notify_all();
}

// Static callback for context state changes
void PulseAudioBackend::context_state_cb(pa_context *c, void *user_data) {
    auto *backend = static_cast<PulseAudioBackend *>(user_data);
    pa_context_state_t state = pa_context_get_state(c);
    if (state == PA_CONTEXT_READY) {
        std::lock_guard<std::mutex> lock(backend->mMutex);
        backend->mContextReadyFlag = true;
        backend->mContextReady.notify_all();
    }
}

PulseAudioBackend::PulseAudioBackend(const std::string &name)
    : mMainloop(pa_mainloop_new(), &PulseAudioBackend::mainloop_deleter),
      mContext(nullptr, &PulseAudioBackend::context_deleter),
      mSinkInfo(nullptr, &PulseAudioBackend::sink_info_deleter),
      mInitialized(false), mName(name) {
    // No initialization here, do it in Initialize()
}

PulseAudioBackend::~PulseAudioBackend() {
    if (mContext) { pa_context_disconnect(mContext.get()); }
    if (mMainloop) { pa_mainloop_quit(mMainloop.get(), 1); }
    if (mMainloopThread.joinable()) { mMainloopThread.join(); }
    mMainloop.reset();
    mContext.reset();
    mSinkInfo.reset();
}

bool PulseAudioBackend::Initialize() {
    std::unique_lock<std::mutex> lock(mMutex);

    if (mInitialized) { return true; }

    if (!mMainloop) { mMainloop.reset(pa_mainloop_new()); }

    pa_context *context = pa_context_new(pa_mainloop_get_api(mMainloop.get()),
                                         "VolumeController");
    if (!context) { return false; }
    mContext.reset(context);

    pa_context_set_state_callback(mContext.get(), context_state_cb, this);

    pa_context_connect(mContext.get(), nullptr, PA_CONTEXT_NOFAIL, nullptr);

    mMainloopThread =
        std::thread([this]() { pa_mainloop_run(mMainloop.get(), nullptr); });

    auto status = mContextReady.wait_for(
        lock, std::chrono::seconds(5), [this]() { return mContextReadyFlag; });

    if (!status) {
        pa_context_disconnect(mContext.get());
        pa_mainloop_quit(mMainloop.get(), 1);
        if (mMainloopThread.joinable()) { mMainloopThread.join(); }
        return false;
    }

    pa_operation *op = pa_context_get_sink_info_by_index(mContext.get(), 0,
                                                         sink_info_cb, this);
    if (!op) { return false; }

    auto sink_status =
        mContextReady.wait_for(lock, std::chrono::seconds(5),
                               [this]() { return mSinkInfo != nullptr; });

    if (!sink_status) { return false; }

    pa_operation_unref(op);

    return mInitialized;
}

bool PulseAudioBackend::IsInitialized() const { return mInitialized; }

float PulseAudioBackend::ReadVolume() const {
    if (!mInitialized || !mSinkInfo) { return 0.0f; }

    std::lock_guard<std::mutex> lock(mMutex);
    float volume = 0.0f;

    if (mSinkInfo) {
        const pa_cvolume &vol = mSinkInfo->volume;
        uint32_t total = 0;
        for (uint32_t i = 0; i < vol.channels; ++i) {
            total += vol.values[i];
        }
        volume = static_cast<float>(total) /
                 (static_cast<float>(vol.channels) * PA_VOLUME_NORM);
    }

    return volume;
}

void PulseAudioBackend::SetVolume(float volume) {
    if (!mInitialized || !mContext || !mSinkInfo) { return; }

    std::lock_guard<std::mutex> lock(mMutex);
    pa_cvolume vol;
    pa_cvolume_set(&vol, mSinkInfo->channel_map.channels,
                   static_cast<pa_volume_t>(volume * PA_VOLUME_NORM));

    // Apply the volume to the sink
    pa_operation *op = pa_context_set_sink_volume_by_index(
        mContext.get(), mSinkInfo->index, &vol, nullptr, nullptr);
    if (op) { pa_operation_unref(op); }
}

void PulseAudioBackend::Mute(bool enabled) {
    if (!mInitialized || !mContext || !mSinkInfo) { return; }

    std::lock_guard<std::mutex> lock(mMutex);
    pa_operation *op = pa_context_set_sink_mute_by_index(
        mContext.get(), mSinkInfo->index, enabled, nullptr, nullptr);
    if (!op) { return; }
    pa_operation_unref(op);
}
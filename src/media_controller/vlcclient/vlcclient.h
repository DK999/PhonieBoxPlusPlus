#ifndef VLC_CLIENT_H
#define VLC_CLIENT_H

#pragma once

#include <cstdint>
#include <functional>
#include <mutex>
#include <string>

extern "C" {
#include <vlc/vlc.h>
}

enum class VlcState { Stopped, Playing, Paused, Error };

struct VlcClientPrivate {
    libvlc_instance_t *m_instance = nullptr;
    libvlc_media_player_t *m_player = nullptr;
};

/// @brief Wraps the libvlc C API with C++ callbacks.
class VlcClient {
  public:
    VlcClient();
    ~VlcClient();

    // Non-copyable, non-movable
    VlcClient(const VlcClient &) = delete;
    VlcClient &operator=(const VlcClient &) = delete;
    VlcClient(VlcClient &&) = delete;
    VlcClient &operator=(VlcClient &&) = delete;

    bool play(const std::string &path);
    bool pause();
    bool resume();
    bool stop();
    bool rewind(double seconds);
    bool fastforward(double seconds);
    int64_t getPlaytime() const;
    int64_t getPlaytimeLeft() const;
    bool isPlaying() const;
    VlcState getState() const;

    void setPlaybackStartedCallback(std::function<void()> cb);
    void setPlaybackPausedCallback(std::function<void()> cb);
    void setPlaybackStoppedCallback(std::function<void()> cb);
    void setPlaybackEndedCallback(std::function<void()> cb);
    void setErrorOccurredCallback(std::function<void(const std::string &)> cb);
    void setStateChangedCallback(std::function<void(VlcState)> cb);

  private:
    void setupEventCallbacks();
    static void vlcEventCallback(const libvlc_event_t *event, void *data);
    void handleMediaPlayerPlaying();
    void handleMediaPlayerPaused();
    void handleMediaPlayerStopped();
    void handleMediaPlayerEndReached();
    void handleMediaPlayerEncounteredError();
    void detachEventCallbacks();

    VlcClientPrivate m_vlc;
    VlcState m_state = VlcState::Stopped;
    mutable std::mutex m_mutex;
    libvlc_event_manager_t *m_event_manager = nullptr;
    int m_callbacks[5] = {0, 0, 0, 0, 0};

    std::function<void()> m_cb_playback_started;
    std::function<void()> m_cb_playback_paused;
    std::function<void()> m_cb_playback_stopped;
    std::function<void()> m_cb_playback_ended;
    std::function<void(const std::string &)> m_cb_error_occurred;
    std::function<void(VlcState)> m_cb_state_changed;
};

#endif // VLC_CLIENT_H

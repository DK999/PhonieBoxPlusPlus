#include "vlcclient.h"
#include <iostream>

enum VlcEventIndex {
    Event_Playing = 0,
    Event_Paused,
    Event_Stopped,
    Event_EndReached,
    Event_Error,
    Event_Count
};

VlcClient::VlcClient() : m_state(VlcState::Stopped) {
    m_vlc.m_instance = libvlc_new(0, nullptr);
    if (m_vlc.m_instance == nullptr) {
        std::cerr << "[VlcClient] Failed to create libvlc instance"
                  << std::endl;
        return;
    }

    m_vlc.m_player = libvlc_media_player_new(m_vlc.m_instance);
    if (m_vlc.m_player == nullptr) {
        std::cerr << "[VlcClient] Failed to create media player" << std::endl;
        libvlc_release(m_vlc.m_instance);
        m_vlc.m_instance = nullptr;
        return;
    }

    m_event_manager = libvlc_media_player_event_manager(m_vlc.m_player);
    if (m_event_manager != nullptr) { setupEventCallbacks(); }
}

VlcClient::~VlcClient() {
    detachEventCallbacks();

    if (m_vlc.m_player != nullptr) {
        libvlc_media_player_release(m_vlc.m_player);
        m_vlc.m_player = nullptr;
    }

    if (m_vlc.m_instance != nullptr) {
        libvlc_release(m_vlc.m_instance);
        m_vlc.m_instance = nullptr;
    }
}

bool VlcClient::play(const std::string &path) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_vlc.m_player == nullptr || m_vlc.m_instance == nullptr) {
        return false;
    }

    libvlc_media_t *media =
        libvlc_media_new_path(m_vlc.m_instance, path.c_str());
    if (media == nullptr) {
        std::cerr << "[VlcClient] Failed to create media from path: " << path
                  << std::endl;
        return false;
    }

    libvlc_media_player_set_media(m_vlc.m_player, media);
    libvlc_media_release(media);

    int result = libvlc_media_player_play(m_vlc.m_player);
    if (result != 0) {
        std::cerr << "[VlcClient] Failed to start playback" << std::endl;
        return false;
    }

    m_state = VlcState::Playing;
    return true;
}

bool VlcClient::pause() {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_vlc.m_player == nullptr) { return false; }

    libvlc_media_player_pause(m_vlc.m_player);
    m_state = VlcState::Paused;
    return true;
}

bool VlcClient::resume() {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_vlc.m_player == nullptr) { return false; }

    libvlc_media_player_set_pause(m_vlc.m_player, 0);
    m_state = VlcState::Playing;
    return true;
}

bool VlcClient::stop() {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_vlc.m_player == nullptr) { return false; }

    libvlc_media_player_stop(m_vlc.m_player);

    libvlc_media_t *media = libvlc_media_player_get_media(m_vlc.m_player);
    if (media != nullptr) { libvlc_media_release(media); }

    m_state = VlcState::Stopped;
    return true;
}

bool VlcClient::rewind(double seconds) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_vlc.m_player == nullptr) { return false; }

    int64_t pos = libvlc_media_player_get_time(m_vlc.m_player);
    if (pos < 0) { return false; }

    int64_t new_pos =
        static_cast<int64_t>(std::max(0.0, (pos / 1000.0) - seconds) * 1000);
    libvlc_media_player_set_time(m_vlc.m_player, new_pos);

    return true;
}

bool VlcClient::fastforward(double seconds) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_vlc.m_player == nullptr) { return false; }

    int64_t pos = libvlc_media_player_get_time(m_vlc.m_player);
    if (pos < 0) { return false; }

    int64_t duration = libvlc_media_player_get_length(m_vlc.m_player);
    int64_t new_pos = static_cast<int64_t>(std::min(
        static_cast<double>(duration), ((pos / 1000.0) + seconds) * 1000));
    libvlc_media_player_set_time(m_vlc.m_player, new_pos);

    return true;
}

int64_t VlcClient::getPlaytime() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_vlc.m_player == nullptr) { return -1; }
    return libvlc_media_player_get_time(m_vlc.m_player);
}

int64_t VlcClient::getPlaytimeLeft() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_vlc.m_player == nullptr) { return -1; }
    int64_t length = libvlc_media_player_get_length(m_vlc.m_player);
    int64_t time = libvlc_media_player_get_time(m_vlc.m_player);
    if (length < 0 || time < 0) { return -1; }
    return length - time;
}

bool VlcClient::isPlaying() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_state == VlcState::Playing;
}

VlcState VlcClient::getState() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_state;
}

void VlcClient::setupEventCallbacks() {
    if (!m_event_manager) { return; }

    m_callbacks[Event_Playing] = libvlc_event_attach(
        m_event_manager, libvlc_MediaPlayerPlaying, vlcEventCallback, this);
    m_callbacks[Event_Paused] = libvlc_event_attach(
        m_event_manager, libvlc_MediaPlayerPaused, vlcEventCallback, this);
    m_callbacks[Event_Stopped] = libvlc_event_attach(
        m_event_manager, libvlc_MediaPlayerStopped, vlcEventCallback, this);
    m_callbacks[Event_EndReached] = libvlc_event_attach(
        m_event_manager, libvlc_MediaPlayerEndReached, vlcEventCallback, this);
    m_callbacks[Event_Error] =
        libvlc_event_attach(m_event_manager, libvlc_MediaPlayerEncounteredError,
                            vlcEventCallback, this);
}

void VlcClient::vlcEventCallback(const libvlc_event_t *event, void *data) {
    auto *client = static_cast<VlcClient *>(data);
    switch (event->type) {
    case libvlc_MediaPlayerPlaying:
        client->handleMediaPlayerPlaying();
        break;
    case libvlc_MediaPlayerPaused:
        client->handleMediaPlayerPaused();
        break;
    case libvlc_MediaPlayerStopped:
        client->handleMediaPlayerStopped();
        break;
    case libvlc_MediaPlayerEndReached:
        client->handleMediaPlayerEndReached();
        break;
    case libvlc_MediaPlayerEncounteredError:
        client->handleMediaPlayerEncounteredError();
        break;
    default:
        break;
    }
}

void VlcClient::handleMediaPlayerPlaying() {
    VlcState new_state = VlcState::Playing;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_state = new_state;
    }

    if (m_cb_playback_started) { m_cb_playback_started(); }
    if (m_cb_state_changed) { m_cb_state_changed(new_state); }
}

void VlcClient::handleMediaPlayerPaused() {
    VlcState new_state = VlcState::Paused;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_state = new_state;
    }

    if (m_cb_playback_paused) { m_cb_playback_paused(); }
    if (m_cb_state_changed) { m_cb_state_changed(new_state); }
}

void VlcClient::handleMediaPlayerStopped() {
    VlcState new_state = VlcState::Stopped;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_state = new_state;
    }

    if (m_cb_playback_stopped) { m_cb_playback_stopped(); }
    if (m_cb_state_changed) { m_cb_state_changed(new_state); }
}

void VlcClient::handleMediaPlayerEndReached() {
    VlcState new_state = VlcState::Stopped;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_state = new_state;
    }

    if (m_cb_playback_ended) { m_cb_playback_ended(); }
    if (m_cb_state_changed) { m_cb_state_changed(new_state); }
}

void VlcClient::handleMediaPlayerEncounteredError() {
    VlcState new_state = VlcState::Error;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_state = new_state;
    }

    const char *error_msg = libvlc_errmsg();
    std::string msg;
    if (error_msg != nullptr) {
        msg = error_msg;
        libvlc_clearerr();
    } else {
        msg = "Unknown libvlc error";
    }

    if (m_cb_error_occurred) { m_cb_error_occurred(msg); }
    if (m_cb_state_changed) { m_cb_state_changed(new_state); }
}

void VlcClient::detachEventCallbacks() {
    if (!m_event_manager) { return; }
    for (int i = 0; i < Event_Count; ++i) {
        if (m_callbacks[i] != 0) {
            libvlc_event_detach(m_event_manager,
                                static_cast<libvlc_event_type_t>(i),
                                vlcEventCallback, this);
        }
    }
}

void VlcClient::setPlaybackStartedCallback(std::function<void()> cb) {
    m_cb_playback_started = std::move(cb);
}

void VlcClient::setPlaybackPausedCallback(std::function<void()> cb) {
    m_cb_playback_paused = std::move(cb);
}

void VlcClient::setPlaybackStoppedCallback(std::function<void()> cb) {
    m_cb_playback_stopped = std::move(cb);
}

void VlcClient::setPlaybackEndedCallback(std::function<void()> cb) {
    m_cb_playback_ended = std::move(cb);
}

void VlcClient::setErrorOccurredCallback(
    std::function<void(const std::string &)> cb) {
    m_cb_error_occurred = std::move(cb);
}

void VlcClient::setStateChangedCallback(std::function<void(VlcState)> cb) {
    m_cb_state_changed = std::move(cb);
}

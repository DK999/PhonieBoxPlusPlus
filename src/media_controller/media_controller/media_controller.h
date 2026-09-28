#ifndef MEDIA_CONTROLLER_H
#define MEDIA_CONTROLLER_H

#include "vlcclient.h"
#include <memory>
#include <string>
#include <zmq.hpp>

class MediaController {
  private:
    zmq::context_t m_ctx;

    zmq::socket_t m_media_sub;    // Subscribe to media commands
    zmq::socket_t m_media_status; // Publish status updates

    std::unique_ptr<VlcClient> m_vlc;
    bool m_running;

  public:
    MediaController();
    ~MediaController();

    int run();

  private:
    bool initializeZeroMQ();
    bool initializeVLC();
    bool processMediaEvent(const std::string &command);
    void sendResponse(const std::string &response);

    void onPlaybackStarted();
    void onPlaybackPaused();
    void onPlaybackStopped();
    void onPlaybackEnded();
    void onErrorOccurred(const std::string &message);
    void onStateChanged(VlcState newState);
};

#endif
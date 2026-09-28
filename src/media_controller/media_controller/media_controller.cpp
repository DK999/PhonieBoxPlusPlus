#include "media_controller.h"
#include "ipc_commands.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <thread>
using namespace ipc;
MediaController::MediaController() : m_ctx(1), m_running(true) {
    if (!initializeZeroMQ()) {
        std::cerr << "[Media Controller] Failed to initialize ZeroMQ"
                  << std::endl;
        return;
    }
    initializeVLC();

    m_vlc->setPlaybackStartedCallback([this]() { onPlaybackStarted(); });
    m_vlc->setPlaybackPausedCallback([this]() { onPlaybackPaused(); });
    m_vlc->setPlaybackStoppedCallback([this]() { onPlaybackStopped(); });
    m_vlc->setPlaybackEndedCallback([this]() { onPlaybackEnded(); });
    m_vlc->setErrorOccurredCallback(
        [this](const std::string &msg) { onErrorOccurred(msg); });
    m_vlc->setStateChangedCallback(
        [this](VlcState state) { onStateChanged(state); });
}

MediaController::~MediaController() { m_running = false; }

bool MediaController::initializeZeroMQ() {
    auto media_socket = command_socket_address(System::MEDIA);
    if (!media_socket.has_value()) {
        std::cerr << "[Media Controller] Failed to resolve media socket address"
                  << std::endl;
        return false;
    }

    m_media_sub = zmq::socket_t(m_ctx, ZMQ_SUB);
    m_media_sub.set(zmq::sockopt::subscribe, "");
    m_media_sub.connect(std::string(*media_socket));

    auto status_socket = status_socket_address(System::MEDIA);
    if (!status_socket.has_value()) {
        std::cerr
            << "[Media Controller] Failed to resolve status socket address"
            << std::endl;
        return false;
    }
    m_media_status = zmq::socket_t(m_ctx, ZMQ_PUB);
    m_media_status.connect(std::string(*status_socket));

    return true;
}

bool MediaController::initializeVLC() {
    m_vlc = std::make_unique<VlcClient>();
    std::cout << "[Media Controller] VLC client initialized" << std::endl;
    return true;
}

int MediaController::run() {
    std::cout << "[Media Controller] Started and listening for media events"
              << std::endl;

    while (m_running) {
        zmq::message_t msg;
        auto result = m_media_sub.recv(msg, zmq::recv_flags::none);

        if (result) {
            std::string message(static_cast<char *>(msg.data()), msg.size());
            std::cout << "[Media Controller] Received: " << message
                      << std::endl;
            if (!processMediaEvent(message)) {
                std::cerr << "[Media Controller] Failed to process media event"
                          << std::endl;
            }
        }
    }

    if (m_vlc) { m_vlc->stop(); }
    return 0;
}

bool MediaController::processMediaEvent(const std::string &command) {
    auto inc = IncomingCommand::parse(json::parse(command));

    if (inc.domain == System::UNKNOWN) {
        std::cerr << "[Media Controller] Invalid command: " << command
                  << std::endl;
        return false;
    }

    return std::visit(
        overloaded{
            [&](const MEDIACommand &mc) {
                return std::visit(
                    overloaded{
                        [&](const FILECommand &c) {
                            std::cout
                                << "[Media Controller] Playing: " << c.file
                                << std::endl;
                            return m_vlc->play(c.file);
                        },
                        [&](const STOP_AUDIOCommand &) {
                            std::cout << "[Media Controller] Stopping playback"
                                      << std::endl;
                            m_vlc->stop();
                            return true;
                        },
                        [&](const PAUSE_AUDIOCommand &) {
                            std::cout << "[Media Controller] Pausing playback"
                                      << std::endl;
                            m_vlc->pause();
                            return true;
                        },
                        [&](const RESUME_AUDIOCommand &) {
                            std::cout << "[Media Controller] Resuming playback"
                                      << std::endl;
                            m_vlc->resume();
                            return true;
                        },
                        [&](const NEXT_TITLECommand &) {
                            std::cout << "[Media Controller] NEXT_TITLE "
                                         "(unimplemented)"
                                      << std::endl;
                            return true;
                        },
                        [&](const PREVIOUS_TITLECommand &) {
                            std::cout << "[Media Controller] PREVIOUS_TITLE "
                                         "(unimplemented)"
                                      << std::endl;
                            return true;
                        },
                        [&](const REPEAT_TITLECommand &) {
                            std::cout << "[Media Controller] REPEAT_TITLE "
                                         "(unimplemented)"
                                      << std::endl;
                            return true;
                        },
                    },
                    mc);
            },
            [&](const SYSTEMCommand &sc) {
                return std::visit(
                    overloaded{
                        [&](const SHUTDOWNCommand &) {
                            std::cout << "[Media Controller] Shutdown requested"
                                      << std::endl;
                            m_running = false;
                            return true;
                        },
                        [&](const RELOAD_CONFIGSCommand &) {
                            std::cout << "[Media Controller] RELOAD_CONFIGS "
                                         "(unimplemented)"
                                      << std::endl;
                            return true;
                        },
                        [&](const RELOAD_LIBRARYCommand &) {
                            std::cout << "[Media Controller] RELOAD_LIBRARY "
                                         "(unimplemented)"
                                      << std::endl;
                            return true;
                        },
                    },
                    sc);
            },
            [&](const VOLUMECommand &vc) {
                return std::visit(
                    overloaded{
                        [&](const VOLUME_INCREASECommand &) {
                            std::cout << "[Media Controller] VOLUME_INCREASE "
                                         "(unimplemented)"
                                      << std::endl;
                            return true;
                        },
                        [&](const VOLUME_DECREASECommand &) {
                            std::cout << "[Media Controller] VOLUME_DECREASE "
                                         "(unimplemented)"
                                      << std::endl;
                            return true;
                        },
                        [&](const MUTECommand &) {
                            std::cout
                                << "[Media Controller] MUTE (unimplemented)"
                                << std::endl;
                            return true;
                        },
                        [&](const UNMUTECommand &) {
                            std::cout
                                << "[Media Controller] UNMUTE (unimplemented)"
                                << std::endl;
                            return true;
                        },
                    },
                    vc);
            },
        },
        inc.payload);
}

void MediaController::sendResponse(const std::string &response) {
    m_media_status.send(zmq::buffer(response), zmq::send_flags::none);
}

void MediaController::onPlaybackStarted() {
    std::cout << "[Media Controller] Playback started" << std::endl;
    PLAYBACK_STARTEDStatus status;
    status.sock = System::MEDIA;
    sendResponse(status.toJson().dump());
}

void MediaController::onPlaybackPaused() {
    std::cout << "[Media Controller] Playback paused" << std::endl;
    PLAYBACK_PAUSEDStatus status;
    status.sock = System::MEDIA;
    sendResponse(status.toJson().dump());
}

void MediaController::onPlaybackStopped() {
    std::cout << "[Media Controller] Playback stopped" << std::endl;
    PLAYBACK_ENDEDStatus status;
    status.sock = System::MEDIA;
    sendResponse(status.toJson().dump());
}

void MediaController::onPlaybackEnded() {
    std::cout << "[Media Controller] Playback ended" << std::endl;
    PLAYBACK_ENDEDStatus status;
    status.sock = System::MEDIA;
    sendResponse(status.toJson().dump());
}

void MediaController::onErrorOccurred(const std::string &message) {
    std::cerr << "[Media Controller] VLC error: " << message << std::endl;
    ERRORStatus status;
    status.sock = System::MEDIA;
    status.code = -1;
    status.message = message;
    sendResponse(status.toJson().dump());
}

void MediaController::onStateChanged(VlcState newState) {
    std::cout << "[Media Controller] VLC state changed" << std::endl;
    (void)newState;
}
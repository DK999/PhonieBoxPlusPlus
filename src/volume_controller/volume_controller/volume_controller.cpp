#include "volume_controller.h"
#include "ipc_commands.hpp"
#include "volume_backends/alsa_backend.h"
#include "volume_backends/pulse_audio_backend.h"
#include "volume_backends/volume_backend.h"
#include <chrono>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <sstream>
#include <thread>
#include <zmq.hpp>

using namespace ipc;

std::unique_ptr<VolumeBackend> createVolumeBackend(const std::string &device) {
    auto pulse = std::make_unique<PulseAudioBackend>(device);
    if (pulse->Initialize() && pulse->IsInitialized()) {
        std::cout << "[Volume Control] Using PulseAudio backend" << std::endl;
        return pulse;
    }

    std::cerr << "[Volume Control] PulseAudio unavailable, falling back to ALSA"
              << std::endl;
    return std::make_unique<AlsaBackend>();
}

VolumeControl::VolumeControl(
    std::function<std::shared_ptr<VolumeBackend>(const std::string &)> factory)
    : backend_factory(std::move(factory)), m_ctx(1),
      m_volume_sub(m_ctx, ZMQ_SUB), m_volume_status_pub(m_ctx, ZMQ_PUB) {
    auto vol_socket = command_socket_address(System::VOLUME);
    m_volume_sub.connect(std::string(*vol_socket));
    m_volume_sub.set(zmq::sockopt::subscribe, "");

    auto status_socket = status_socket_address(System::VOLUME);
    if (status_socket) {
        m_volume_status_pub.bind(std::string(*status_socket));
    }
}

bool VolumeControl::initialize(const VolumeControl::VolumeSettings &settings) {
    if (backend_factory) {
        backend = backend_factory(settings.output_device);
    } else {
        auto temp = createVolumeBackend(settings.output_device);
        backend = std::shared_ptr<VolumeBackend>(std::move(temp));
    }

    if (!backend->Initialize()) {
        std::cerr << "[Volume Control] Failed to initialize backend"
                  << std::endl;
        return false;
    }

    if (!backend->IsInitialized()) {
        std::cerr << "[Volume Control] Backend reports not initialized"
                  << std::endl;
        return false;
    }

    current_volume = settings.default_volume;
    backend->SetVolume(current_volume);

    std::cout << "[Volume Control] Initialized with device: "
              << settings.output_device << std::endl;
    return true;
}

void VolumeControl::setVolume(float volume) {
    if (volume < 0.0f) volume = 0.0f;
    if (volume > 1.0f) volume = 1.0f;

    current_volume = volume;

    if (backend && !is_muted) { backend->SetVolume(volume); }

    publishVolumeStatus();
}

float VolumeControl::getVolume() const { return current_volume; }

void VolumeControl::mute() {
    is_muted = true;
    if (backend) { backend->Mute(true); }

    publishVolumeStatus();
}

void VolumeControl::unmute() {
    is_muted = false;
    if (backend) { backend->Mute(false); }

    publishVolumeStatus();
}

void VolumeControl::increaseVolume(float delta) {
    float new_volume = current_volume + delta;
    setVolume(std::min(1.0f, new_volume));
}

void VolumeControl::decreaseVolume(float delta) {
    float new_volume = current_volume - delta;
    setVolume(std::max(0.0f, new_volume));
}

bool VolumeControl::isMuted() const { return is_muted; }

void VolumeControl::publishVolumeStatus() {
    VOLUME_CHANGEDStatus status;
    status.sock = System::VOLUME;
    status.volume = current_volume;
    status.muted = is_muted;

    std::string json_str = status.toJson().dump();
    zmq::message_t pub_msg(json_str.size());
    memcpy(pub_msg.data(), json_str.c_str(), json_str.size());
    m_volume_status_pub.send(pub_msg, zmq::send_flags::none);
}

int VolumeControl::run() {
    std::cout << "Volume Controller started. Subscribing to RFID events...\n";

    while (true) {
        zmq::message_t msg;
        auto result = m_volume_sub.recv(msg, zmq::recv_flags::none);

        if (result) {
            std::string message(static_cast<char *>(msg.data()), msg.size());
            std::cout << "[Volume Control] Received: " << message << std::endl;
            if (!processVolumeCommand(message)) {
                std::cerr << "[Volume Control] Failed to process volume command"
                          << std::endl;
            }
        }
    }

    return 0;
}

bool VolumeControl::processVolumeCommand(const std::string &command) {
    json msg = json::parse(command);
    std::cout << "Vol: Received JSON:" << msg.dump() << std::endl;
    auto inc = IncomingCommand::parse(msg);

    if (inc.domain != System::VOLUME) {
        std::cerr << "[Volume Control] Non-volume command received (domain="
                  << static_cast<int>(inc.domain) << ")" << std::endl;
        return false;
    }

    std::visit(
        overloaded{
            [&](const VOLUME_INCREASECommand &c) { increaseVolume(c.amount); },
            [&](const VOLUME_DECREASECommand &c) { decreaseVolume(c.amount); },
            [&](const MUTECommand &) { mute(); },
            [&](const UNMUTECommand &) {
                unmute();
                std::cout << "[Volume Control] Unmuted audio output"
                          << std::endl;
            },
        },
        std::get<VOLUMECommand>(inc.payload));
    return true;
}

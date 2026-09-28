#ifndef volume_controller_H
#define volume_controller_H

#include "volume_backends/volume_backend.h"
#include <functional>
#include <memory>
#include <string>
#include <zmq.hpp>

class VolumeControl {
  public:
    struct VolumeSettings {
        std::string output_device = "default";
        float default_volume = 0.7f;
    };

    VolumeControl(
        std::function<std::shared_ptr<VolumeBackend>(const std::string &)>
            backend_factory = nullptr);

    bool initialize(const VolumeSettings &settings);
    void setVolume(float volume);
    float getVolume() const;
    void mute();
    void unmute();
    void increaseVolume(float delta);
    void decreaseVolume(float delta);
    bool isMuted() const;
    int run();
    VolumeBackend *GetBackend() { return backend.get(); }

  private:
    bool processVolumeCommand(const std::string &command);
    void publishVolumeStatus();

    std::shared_ptr<VolumeBackend> backend;
    VolumeSettings settings;
    float current_volume = 1.0f;
    bool is_muted = false;

    std::function<std::shared_ptr<VolumeBackend>(const std::string &)>
        backend_factory;

    zmq::context_t m_ctx;
    zmq::socket_t m_volume_sub;
    zmq::socket_t m_volume_status_pub;
};

#endif // volume_controller_H

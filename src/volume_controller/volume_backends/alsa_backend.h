#ifndef ALSA_BACKEND_H
#define ALSA_BACKEND_H

#include "volume_backend.h"

class AlsaBackend : public VolumeBackend {
  public:
    AlsaBackend() = default;
    ~AlsaBackend() override = default;

    bool Initialize() override;
    bool IsInitialized() const override;
    float ReadVolume() const override;
    void SetVolume(float volume) override;
    void Mute(bool enabled) override;

  private:
    bool mInitialized = false;
};

#endif // ALSA_BACKEND_H
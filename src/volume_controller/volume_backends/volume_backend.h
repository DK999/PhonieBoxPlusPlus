#ifndef VOLUME_BACKEND_H
#define VOLUME_BACKEND_H

#include <string>

class VolumeBackend {
  public:
    virtual ~VolumeBackend() = default;
    virtual bool Initialize() = 0;
    virtual bool IsInitialized() const = 0;
    virtual float ReadVolume() const = 0;
    virtual void SetVolume(float volume) = 0;
    virtual void Mute(bool enabled) = 0;
};

#endif // VOLUME_BACKEND_H

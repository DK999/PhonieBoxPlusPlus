#include "alsa_backend.h"
#include <iostream>

bool AlsaBackend::Initialize() {
    std::cerr << "[AlsaBackend] Not yet implemented." << std::endl;
    mInitialized = false;
    return false;
}

bool AlsaBackend::IsInitialized() const { return mInitialized; }

float AlsaBackend::ReadVolume() const { return 0.0f; }

void AlsaBackend::SetVolume(float volume) {
    std::cerr << "[AlsaBackend] SetVolume(" << volume
              << ") — not yet implemented." << std::endl;
}

void AlsaBackend::Mute(bool enabled) {
    std::cerr << "[AlsaBackend] Mute(" << (enabled ? "true" : "false")
              << ") — not yet implemented." << std::endl;
}
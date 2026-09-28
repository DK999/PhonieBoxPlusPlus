#include "volume_controller/volume_controller.h"

int main() {
    VolumeControl volume_controller;

    VolumeControl::VolumeSettings settings;
    settings.output_device = "default";
    settings.default_volume = 0.5f;

    if (!volume_controller.initialize(settings)) { return 1; }

    return volume_controller.run();
}
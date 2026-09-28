#pragma once

#include "../Irfid_reader.h"
#include <functional>
#include <string>

class HidRfidReader : public Irfid_reader {
  private:
    int device_fd;
    std::string device_path;
    std::string last_tag;
    bool running;
    std::function<void(const std::string &tag)> tag_callback;

  public:
    explicit HidRfidReader(
        const std::string &path = "/dev/input/event237",
        std::function<void(const std::string &tag)> callback = nullptr);
    ~HidRfidReader() override;

    bool init() override;
    std::string getID() override;
    void readFromDevice() override;
    void stop() override;

  private:
    void publishRfidTag(const std::string &tag);
};

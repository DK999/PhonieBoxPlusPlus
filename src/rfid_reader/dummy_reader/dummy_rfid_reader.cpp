#include "dummy_rfid_reader.h"
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <thread>
#include <unistd.h>

DummyRfidReader::DummyRfidReader(
    const std::string &path,
    std::function<void(const std::string &tag)> callback)
    : device_fd(-1), device_path(path), running(false), tag_callback(callback) {
}

DummyRfidReader::~DummyRfidReader() {
    if (device_fd >= 0) { close(device_fd); }
}

bool DummyRfidReader::init() {
    if (mkfifo(device_path.c_str(), 0666) < 0) {
        if (errno != EEXIST) {
            std::cerr << "[RFID Reader] Failed to create named pipe: "
                      << strerror(errno) << std::endl;
            return false;
        }
    }

    device_fd = open(device_path.c_str(), O_RDONLY | O_NONBLOCK);
    if (device_fd < 0) {
        std::cerr << "[RFID Reader] Failed to open named pipe: "
                  << strerror(errno) << std::endl;
        return false;
    }

    std::cout << "[RFID Reader] Named pipe " << device_path
              << " created and opened successfully" << std::endl;
    running = true;
    return true;
}

std::string DummyRfidReader::getID() { return last_tag; }

void DummyRfidReader::readFromDevice() {
    if (device_fd < 0) {
        std::cerr << "[RFID Reader] Device not initialized" << std::endl;
        return;
    }

    std::cout << "[RFID Reader] Waiting for RFID scan data..." << std::endl;

    char buffer[1024];
    while (running) {
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(device_fd, &read_fds);

        struct timeval timeout;
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;

        int ready = select(device_fd + 1, &read_fds, NULL, NULL, &timeout);

        if (ready == -1) {
            if (errno == EINTR) continue;
            std::perror("[RFID Reader] Error in select");
            break;
        }

        if (ready == 0) { continue; }

        ssize_t bytes = read(device_fd, buffer, sizeof(buffer) - 1);
        if (bytes < 0) {
            std::perror("[RFID Reader] Error reading from named pipe");
            break;
        }

        if (bytes > 0) {
            buffer[bytes] = '\0';
            std::string tag(buffer);
            tag.erase(tag.find_last_not_of(" \n\r\t") + 1);

            if (!tag.empty()) {
                last_tag = tag;
                publishRfidTag(last_tag);
                std::cout << "[RFID Reader] Received tag: " << tag << std::endl;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
}

void DummyRfidReader::publishRfidTag(const std::string &tag) {
    if (tag_callback) { tag_callback(tag); }
}

void DummyRfidReader::stop() { running = false; }
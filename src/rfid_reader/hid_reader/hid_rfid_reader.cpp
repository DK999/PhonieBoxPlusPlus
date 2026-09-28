#include "hid_rfid_reader.h"
#include <fcntl.h>
#include <iostream>
#include <linux/input-event-codes.h>
#include <linux/input.h>
#include <map>
#include <sys/ioctl.h>
#include <unistd.h>

static std::map<int, std::string> KEY_MAP = []() {
    std::map<int, std::string> m;

    m[KEY_1] = "1";
    m[KEY_2] = "2";
    m[KEY_3] = "3";
    m[KEY_4] = "4";
    m[KEY_5] = "5";
    m[KEY_6] = "6";
    m[KEY_7] = "7";
    m[KEY_8] = "8";
    m[KEY_9] = "9";
    m[KEY_0] = "0";

    m[KEY_A] = "A";
    m[KEY_B] = "B";
    m[KEY_C] = "C";
    m[KEY_D] = "D";
    m[KEY_E] = "E";
    m[KEY_F] = "F";
    m[KEY_G] = "G";
    m[KEY_H] = "H";
    m[KEY_I] = "I";
    m[KEY_J] = "J";
    m[KEY_K] = "K";
    m[KEY_L] = "L";
    m[KEY_M] = "M";
    m[KEY_N] = "N";
    m[KEY_O] = "O";
    m[KEY_P] = "P";
    m[KEY_Q] = "Q";
    m[KEY_R] = "R";
    m[KEY_S] = "S";
    m[KEY_T] = "T";
    m[KEY_U] = "U";
    m[KEY_V] = "V";
    m[KEY_W] = "W";
    m[KEY_X] = "X";
    m[KEY_Y] = "Y";
    m[KEY_Z] = "Z";

    m[KEY_MINUS] = "-";
    m[KEY_EQUAL] = "=";
    m[KEY_SPACE] = " ";
    m[KEY_COMMA] = "<";
    m[KEY_DOT] = ">";
    m[KEY_SLASH] = "?";
    m[KEY_SEMICOLON] = ":";

    return m;
}();

HidRfidReader::HidRfidReader(
    const std::string &path,
    std::function<void(const std::string &tag)> callback)
    : device_fd(-1), device_path(path), running(false), tag_callback(callback) {
}

HidRfidReader::~HidRfidReader() {
    if (device_fd >= 0) { close(device_fd); }
}

bool HidRfidReader::init() {
    device_fd = open(device_path.c_str(), O_RDONLY);
    if (device_fd < 0) {
        std::perror(
            "[RFID Reader] Failed to open device. Try running with sudo or "
            "add user to 'input' group");
        return false;
    }

    unsigned char keybit[256] = {0};
    if (ioctl(device_fd, EVIOCGBIT(EV_KEY, sizeof(keybit)), keybit) < 0) {
        std::cerr
            << "[RFID Reader] Device does not support EV_KEY events. Not a "
               "valid HID input device."
            << std::endl;
        close(device_fd);
        device_fd = -1;
        return false;
    }

    std::cout << "[RFID Reader] Device " << device_path
              << " opened successfully" << std::endl;
    running = true;
    return true;
}

std::string HidRfidReader::getID() { return last_tag; }

void HidRfidReader::readFromDevice() {
    if (device_fd < 0) {
        std::cerr << "[RFID Reader] Device not initialized" << std::endl;
        return;
    }

    std::string buffer;
    struct input_event ev;

    std::cout
        << "[RFID Reader] Waiting for RFID scan (buffers until Enter key)..."
        << std::endl;

    while (running) {
        ssize_t bytes = read(device_fd, &ev, sizeof(ev));
        if (bytes < 0) {
            if (errno == EINTR) continue;
            std::perror("[RFID Reader] Error reading device");
            break;
        }
        if (static_cast<size_t>(bytes) < sizeof(ev)) continue;

        if (ev.type != EV_KEY) continue;
        if (ev.value != 1) continue;

        if (ev.code == KEY_ENTER) {
            if (!buffer.empty()) {
                last_tag = buffer;
                publishRfidTag(last_tag);
            } else {
                std::cout << "[RFID Reader] Empty scan received" << std::endl;
            }
            buffer.clear();
        } else if (KEY_MAP.count(ev.code)) {
            buffer += KEY_MAP.at(ev.code);
        }
    }
}

void HidRfidReader::publishRfidTag(const std::string &tag) {
    if (tag_callback) { tag_callback(tag); }
}

void HidRfidReader::stop() { running = false; }
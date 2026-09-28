# HidRfidReader Implementation

This directory contains the implementation of a USB HID-based RFID reader that listens to input devices such as `/dev/input/event237`.

## Files

- **hid_rfid_reader.h**: Header file containing the `HidRfidReader` class declaration
- **hid_rfid_reader.cpp**: Implementation of the `HidRfidReader` class
- **README.md**: This file

## Features

- Implements the `Irfid_reader` interface
- Listens to Linux input events from HID devices
- Supports standard QWERTY keyboard layout (digits, uppercase letters, and common special characters)
- Buffers input until Enter key is pressed
- Publishes scanned RFID tags via ZeroMQ PUB socket (ipc:///tmp/rfid_reader.ipc)
- Proper error handling and device validation

## Usage

The RFID reader can be used as a standalone executable or integrated into the larger PhonieBox++ system.

### Command Line Usage

```bash
./build/Release/rfid_reader/rfid_reader [device_path]
```

- `device_path`: Optional path to the input device (default: `/dev/input/event237`)

### Integration

To use this reader in your application:

```cpp
#include "hid_reader/hid_rfid_reader.h"

HidRfidReader reader("/dev/input/event237");
if (reader.init()) {
    // Start reading in a separate thread
    std::thread reader_thread([&reader]() {
        reader.readFromDevice();
    });

    // Your application logic here

    reader.stop();
    reader_thread.join();
}
```

## Dependencies

- ZeroMQ (libzmq) for inter-process communication
- Linux input event system (requires `rt` library)

## Notes

- The application needs read access to the input device. You may need to:
  - Run as root (sudo)
  - Add your user to the 'input' group: `sudo usermod -a -G input $USER`
  - Set appropriate permissions on the device

## Key Mapping

The reader supports the following key mappings:
- Digits: 0-9
- Uppercase letters: A-Z
- Special characters: - = < > ? :
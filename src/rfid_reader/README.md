# HID-Based RFID Reader Implementation

This implementation provides a USB HID-based RFID reader that listens to Linux input devices (e.g., `/dev/input/event237`) and publishes scanned RFID tags via ZeroMQ.

## Features

✅ **HID Input Device Support**: Listens to standard Linux input devices
✅ **ZeroMQ Integration**: Publishes tags via IPC socket
✅ **Clean Tag Publishing**: Only the tag is published (no prefix like "RFID.scanned:")
✅ **Detailed Logging**: Logs scanned tags with descriptive messages
✅ **Configurable Device Path**: Can specify any `/dev/input/eventX` device
✅ **Interface Compliance**: Implements the `Irfid_reader` interface

## Implementation Details

### Key Components

1. **`Irfid_reader` Interface** (`rfid_reader/Irfid_reader.h`)
   - Virtual base class for RFID readers
   - `init()`: Initialize the reader
   - `getID()`: Retrieve the last scanned RFID tag (returns `std::string`)

2. **`HidRfidReader` Class** (`rfid_reader/rfid_reader.cpp`)
   - Implements `Irfid_reader` interface
   - Uses Linux input event API to read from `/dev/input/eventX`
   - Buffers characters until Enter key is pressed
   - Publishes complete RFID tag via ZeroMQ

### How It Works

1. **Device Initialization**: Opens the specified input device
2. **Event Processing**: Reads `EV_KEY` events from the device
3. **Key Mapping**: Converts key codes to characters using standard QWERTY layout
4. **Buffering**: Accumulates characters until Enter key is pressed
5. **Publishing**: Sends the complete RFID tag via ZeroMQ (no prefix)
6. **Logging**: Outputs detailed information about scanned tags

### ZeroMQ Configuration

- **Socket Type**: PUB (publish-subscribe)
- **Binding**: `ipc:///tmp/rfid_reader.ipc`
- **Message Format**: Raw RFID tag string (no prefix, no suffix)

## Usage

### Running the RFID Reader

```bash
# Default device (/dev/input/event237)
./build/Release/rfid_reader/rfid_reader

# Custom device
./build/Release/rfid_reader/rfid_reader /dev/input/eventX
```

### Building

```bash
cmake --build build/Release --target rfid_reader
```

## Requirements

- Linux system with input event devices
- ZeroMQ library (libzmq-dev)
- C++17 or later
- Access to `/dev/input/event*` devices (may require sudo or adding user to 'input' group)

## Example Output

```
[RFID Reader] Initialized and bound to IPC socket
[RFID Reader] Device /dev/input/event237 opened successfully
[RFID Reader] Waiting for RFID scan (buffers until Enter key)...
[RFID Reader] Scanned tag: ABC123
```

## Integration with Other Components

The RFID reader publishes tags via ZeroMQ, which can be subscribed to by other components:

```cpp
zmq::context_t context(1);
zmq::socket_t subscriber(context, ZMQ_SUB);
subscriber.connect("ipc:///tmp/rfid_reader.ipc");
subscriber.setsockopt(ZMQ_SUBSCRIBE, "", 0);

// Receive RFID tags
zmq::message_t msg;
subscriber.recv(msg);
std::string tag(static_cast<char*>(msg.data()), msg.size());
// tag now contains the RFID tag (e.g., "ABC123")
```

## Notes

- The reader buffers characters until Enter key is pressed
- Only key press events (value=1) are processed
- Standard QWERTY keyboard layout is used for key mapping
- Empty scans (Enter without any characters) are logged but not published
- The implementation includes proper error handling and device cleanup
# PhonieBox++

A C++20 Media Player in the style of original PhonieBox project.
It's rather a C++ playing ground for me to have a reason to update my programming skills :) 

## Architecture

```
[RFID Reader] -> [RFID Parser] --> [Media Controller]
                               \-> [Volume Controller]
```

- **RFID Reader** — reads tag IDs from HID devices (or a dummy reader) and publishes raw tag events via ZeroMQ.
- **RFID Parser** — subscribes to raw tag events, looks up the tag in JSON configuration files, resolves the corresponding IPC command, and publishes the typed command to the appropriate subsystem.
- **Media Controller** — handles media playback commands (play, pause, stop, skip) via libVLC.
- **Volume Controller** — handles volume changes (increase, decrease, mute, unmute) via ALSA or PulseAudio backends.

All subsystems communicate through typed IPC messages defined in `config/ipc_command_schema.json` and generated at build time as `ipc_commands.hpp`.

---

## Dependencies

### Arch Linux

```bash
sudo pacman -S cmake ninja clang nlohmann-json zeromq alsa-lib \
  pulseaudio-libs jsoncpp libvlc gtest python
```

### Debian / Ubuntu

```bash
sudo apt install cmake ninja-build clang nlohmann-json3-dev \
  libzmq3-dev libasound2-dev libpulse-dev libjsoncpp-dev \
  vlc libvlc-dev libgtest-dev python3
```

> **Note:** On Debian-based systems, GTest must be built from source after installation:
> ```bash
> sudo cmake -S /usr/src/googletest -B /tmp/gtest-build -DBUILD_GMOCK=ON
> sudo cmake --build /tmp/gtest-build
> sudo cmake --install /tmp/gtest-build

---

## Building & Running

### CMake Presets

This project ships with CMake presets for streamlined configuration and builds. Presets are located in `CMakePresets.json`.

#### Configuration Presets

| Preset       | Description                         | Compiler | Build Type |
|-------------|-------------------------------------|----------|------------|
| `Release`   | Optimized release build             | clang++  | Release    |
| `Debug`     | Unoptimized debug build with symbols| clang++  | Debug      |

#### Build Presets

| Preset        | Description              | Targets                        |
|--------------|--------------------------|--------------------------------|
| `Release`    | Build release artifacts  | install, all libraries, binaries |
| `Debug`      | Build debug artifacts    | install, all libraries, binaries |

#### Test Presets

| Preset           | Description              | Configure Preset |
|-----------------|--------------------------|-----------------|
| `Test Release all` | Run all tests (release) | `Release`       |
| `Test Debug all`   | Run all tests (debug)   | `Debug`         |

#### Quick Start

```bash
# Configure with the Release preset
cmake --preset=Release

# Build
cmake --build --preset Release

# Run tests
ctest --preset "Test Release all"
```
#### Manual Configuration

```bash
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++
cmake --build .
```

---

## Dummy RFID Reader

The project includes a **dummy RFID reader** for testing without a physical HID RFID device. It simulates an RFID reader by reading tag IDs from a Unix named pipe (FIFO).

### How it works

1. On startup, the dummy reader creates a named pipe (default: `/tmp/dummyrfid`).
2. It opens the pipe for non-blocking reads and waits for data using `select()` with a 1-second timeout.
3. When a tag ID is written to the pipe, the reader reads it, strips trailing whitespace, and publishes it as an RFID event via ZeroMQ.

### Usage

Build and run the `rfid_reader` binary. It accepts an optional argument for the pipe path:

```bash
# Use default pipe path: /tmp/dummyrfid
.rfid_reader

# Use a custom pipe path
.rfid_reader /tmp/my_custom_pipe
```

In a separate terminal, simulate RFID scans by writing tag IDs to the pipe:

```bash
# Write a tag ID to the pipe (non-blocking — use & to avoid hanging)
echo "01" > /tmp/dummyrfid &
echo "20" > /tmp/dummyrfid &
echo "90" > /tmp/dummyrfid &
```

Each `echo` sends one tag ID. The reader processes them as they arrive and dispatches the corresponding IPC commands to the appropriate subsystem.

### Architecture note

The dummy reader implements the same `Irfid_reader` interface as the real HID reader, so it can be swapped in as a drop-in replacement. The actual reader selection (HID vs. dummy) is currently hardcoded in `main.cpp` — both implementations are compiled into the same binary.

---

## RFID Tag Configuration

The RFID parser reads four JSON configuration files from the project's `config/` directory. Each file maps RFID tag IDs (strings) to specific command types. The parser checks them in priority order: **media** → **volume** → **system**.

### File Overview

| File               | Subsystem | Commands                                    |
|--------------------|-----------|---------------------------------------------|
| `media.json`       | MEDIA     | Playback control (stop, pause, resume, skip) |
| `volume.json`      | VOLUME    | Volume adjustment and mute/unmute            |
| `library.json`     | MEDIA     | Play individual audio files                  |
| `system.json`      | SYSTEM    | Shutdown, reload configs/library             |

### `media.json` — Playback Controls

```json
{
    "tags": {
        "20": { "type": "STOP_AUDIO",       "description": "Stop Playback" },
        "21": { "type": "PAUSE_AUDIO",      "description": "Pause Playback" },
        "22": { "type": "RESUME_AUDIO",     "description": "Resume playback" },
        "23": { "type": "NEXT_TITLE",       "description": "Skip to next track" },
        "24": { "type": "PREVIOUS_TITLE",   "description": "Jump to previous track" },
        "25": { "type": "REPEAT_TITLE",     "description": "Start from beginning" }
    },
}
```

Each entry maps a **tag ID** (the string read from the RFID card) to a **command type**. Supported types:

| Type             | Parameters   | Description                          |
|------------------|-------------|--------------------------------------|
| `STOP_AUDIO`     | —           | Stop current playback                |
| `PAUSE_AUDIO`    | —           | Pause current playback               |
| `RESUME_AUDIO`   | —           | Resume paused playback               |
| `NEXT_TITLE`     | —           | Skip to the next track               |
| `PREVIOUS_TITLE` | —           | Jump to the previous track           |
| `REPEAT_TITLE`   | `enabled` (bool) | Restart current track from beginning |

### `volume.json` — Volume Controls

```json
{
    "tags": {
        "00": { "type": "VOLUME_INCREASE", "amount": 0.05, "description": "Increase volume by 5%" },
        "01": { "type": "VOLUME_DECREASE", "amount": 0.05, "description": "Decrease volume by 5%" },
        "02": { "type": "MUTE",            "description": "Mute audio" },
        "03": { "type": "UNMUTE",          "description": "Unmute audio" }
    }
}
```

| Type                | Parameters | Description                          |
|---------------------|-----------|--------------------------------------|
| `VOLUME_INCREASE`   | `amount` (number, default 0.1) | Increase volume by the specified amount |
| `VOLUME_DECREASE`   | `amount` (number, default 0.1) | Decrease volume by the specified amount |
| `MUTE`              | —         | Toggle mute on                       |
| `UNMUTE`            | —         | Toggle mute off                      |

### `library.json` — File Playback

```json
{
    "tags": {
        "10": {
            "type": "FILE",
            "file": "/usr/share/sounds/alert.wav",
            "description": "Piper - Cool Selection - Summer Breeze"
        }
    }
}
```

| Type | Parameters       | Description                           |
|------|-----------------|---------------------------------------|
| `FILE` | `file` (string, required), `loop` (bool, default false) | Play the specified audio file |

The `file` field must be an absolute path to an audio file. Set `loop: true` to repeat playback.

### `system.json` — System Commands

```json
{
    "tags": {
        "90": { "type": "SHUTDOWN" },
        "91": { "type": "RELOAD_CONFIGS" },
        "92": { "type": "RELOAD_LIBRARY" }
    }
}
```

| Type               | Parameters     | Description                              |
|--------------------|---------------|------------------------------------------|
| `SHUTDOWN`         | `delay_seconds` (int, default 0) | Shut down the system after an optional delay |
| `RELOAD_CONFIGS`   | —             | Reload all configuration files             |
| `RELOAD_LIBRARY`   | —             | Reload the library configuration           |

### Tag ID Range Guidelines

| Prefix | Subsystem    | Example |
|--------|-------------|---------|
| 00–09  | VOLUME      | `00` = increase volume |
| 10–19  | MEDIA (file)| `10` = play a specific file |
| 20–29  | MEDIA (ctrl)| `20` = stop playback |
| 90–99  | SYSTEM      | `90` = shutdown |

This is a convention — any string tag ID works in any file, but keeping tags in these ranges makes it easy to identify their purpose at a glance.

---

## IPC Communication

All subsystems communicate via ZeroMQ IPC sockets defined in `config/ipc_command_schema.json`:

| System       | Command Socket                   | Status Socket                     |
|-------------|----------------------------------|-----------------------------------|
| SYSTEM      | `ipc:///tmp/system_commands.ipc` | `ipc:///tmp/system_status.ipc`    |
| VOLUME      | `ipc:///tmp/volume_commands.ipc` | `ipc:///tmp/volume_status.ipc`    |
| MEDIA       | `ipc:///tmp/media_commands.ipc`  | `ipc:///tmp/media_status.ipc`     |
| LIBRARY     | `ipc:///tmp/library_commands.ipc`| `ipc:///tmp/library_status.ipc`   |
| RFID_READER | `ipc:///tmp/rfid_reader_commands.ipc` | `ipc:///tmp/rfid_reader_status.ipc` |
| WEB         | `ipc:///tmp/web_commands.ipc`    | `ipc:///tmp/web_status.ipc`       |

---

## Project Structure

```
PhonieBox++/
├── CMakeLists.txt              # Root CMake configuration
├── CMakePresets.json           # CMake presets (Release, Debug)
├── config/
│   ├── ipc_command_schema.json # IPC command & status type definitions
│   ├── library.json            # RFID → file playback mappings
│   ├── media.json              # RFID → media control mappings
│   ├── system.json             # RFID → system command mappings
│   └── volume.json             # RFID → volume control mappings
├── src/
│   ├── main.cpp                # Main entry point
│   ├── rfid_reader/            # HID / dummy RFID reader
│   ├── rfid_parser/            # Tag lookup → IPC command dispatch
│   ├── media_controller/       # libVLC-based media playback
│   └── volume_controller/      # ALSA / PulseAudio volume control
├── tests/
│   ├── unit/                   # Unit tests (GTest)
│   └── integration/            # Integration tests (GTest)
└── tools/
    └── generate_command_types.py  # Code generator for ipc_commands.hpp
```

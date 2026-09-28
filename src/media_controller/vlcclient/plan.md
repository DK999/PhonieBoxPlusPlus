# VlcClient — libvlc wrapper with event callbacks

## Goal

Create a standalone `VlcClient` class that wraps the libvlc C API and exposes:

- **Playback control**: `play(path)`, `pause()`, `resume()`, `stop()`
- **Seeking**: `rewind(seconds)`, `fastforward(seconds)`
- **Time queries**: `getPlaytime()`, `getPlaytimeLeft()`
- **Event callbacks** (signals): playback started/paused/stopped/ended, error occurred, general state change

## Architecture

```
┌─────────────────────────────────────────────────┐
│                    VlcClient                     │
│                                                  │
│  Public API:                                     │
│    play(path) → bool                             │
│    pause() / resume() / stop()                   │
│    rewind(s) / fastforward(s)                    │
│    getPlaytime() → int64_t                       │
│    getPlaytimeLeft() → int64_t                   │
│    isPlaying() → bool                            │
│                                                  │
│  Signals (event callbacks):                      │
│    playbackStarted()                             │
│    playbackPaused()                              │
│    playbackStopped()                             │
│    playbackEnded()                               │
│    errorOccurred(message)                        │
│    stateChanged(newState)                        │
│                                                  │
│  Private:                                        │
│    m_vlc — opaque libvlc instance/player wrapper │
│    m_state — current playback state              │
│    m_mutex — thread safety for state access      │
│    setupEventCallbacks() — registers libvlc callbacks → signals │
└──────────────────────────────────────────────────┘
```

## Key design decisions

1. **Initialization in constructor** — libvlc instance + media player + event callbacks are all set up during construction. No separate `init()` call needed.

2. **Opaque pointer for libvlc internals** — `m_vlc` is an opaque `struct` containing `libvlc_instance_t*` and `libvlc_media_player_t*`. The C++ code doesn't expose libvlc types in the header.

3. **Full libvlc event callbacks** — libvlc event manager (`libvlc_event_manager_t`) is queried via `libvlc_event_attach()` for these event types:
   - `libvlc_MediaPlayerPlaying` → `playbackStarted()` + `stateChanged(Playing)`
   - `libvlc_MediaPlayerPaused` → `playbackPaused()` + `stateChanged(Paused)`
   - `libvlc_MediaPlayerStopped` → `playbackStopped()` + `stateChanged(Stopped)`
   - `libvlc_MediaPlayerEndReached` → `playbackEnded()` + `stateChanged(Stopped)`
   - `libvlc_MediaPlayerEncounteredError` → `errorOccurred(msg)` + `stateChanged(Error)`

4. **Static callback bridge** — libvlc callbacks are C-style function pointers. A static method `VlcClient::vlcEventCallback()` casts the opaque context pointer back to `VlcClient*` and dispatches to the appropriate signal handler.

5. **State enum** — `VlcState` enum (`Stopped`, `Playing`, `Paused`, `Error`) tracks current playback state, updated by both signals and method calls.

6. **Threading** — libvlc callbacks fire on libvlc's internal thread. Signals are emitted directly; consumers should use `Qt::QueuedConnection` (or equivalent) if connecting from a different thread.

## File structure

| File | Contents |
|---|---|
| `vlcclient.h` | `#pragma once`, libvlc C headers (extern "C"), `VlcClient` class declaration with signals, `VlcState` enum, opaque `VlcClientPrivate` struct |
| `vlcclient.cpp` | Constructor/destructor, all method implementations, static callback, event setup |
| `CMakeLists.txt` (media_controller) | New `vlcclient` library target, linked to `libvlc`, added as `add_subdirectory` |

## libvlc API mapping

| VlcClient method | libvlc function(s) |
|---|---|
| Constructor | `libvlc_new()`, `libvlc_media_player_new()`, `libvlc_media_player_event_manager()`, `libvlc_event_attach()` ×5 |
| `play(path)` | `libvlc_media_new_path()`, `libvlc_media_player_set_media()`, `libvlc_media_player_play()` |
| `pause()` | `libvlc_media_player_pause()` |
| `resume()` | `libvlc_media_player_set_pause(player, false)` |
| `stop()` | `libvlc_media_player_stop()`, `libvlc_media_release()` |
| `rewind(s)` | `libvlc_media_player_get_time()` + `libvlc_media_player_set_time(time - s*1000)` |
| `fastforward(s)` | `libvlc_media_player_get_time()` + `libvlc_media_player_set_time(time + s*1000)` |
| `getPlaytime()` | `libvlc_media_player_get_time()` |
| `getPlaytimeLeft()` | `libvlc_media_player_get_length() - libvlc_media_player_get_time()` |
| `isPlaying()` | `libvlc_media_player_is_playing()` |
| Destructor | `libvlc_media_player_release()`, `libvlc_release()` |

## What I won't do (out of scope)

- No integration into `MediaController` yet — that comes later
- No volume control (handled by `VolumeControl` separately)
- No playlist support (media_controller will handle that)
- No video rendering (audio-only use case)
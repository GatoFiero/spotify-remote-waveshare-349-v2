#pragma once
// Everything that talks to Spotify, on its own task pinned to core 0 so a
// 500 ms HTTPS round trip never stalls touch or drawing on core 1.

#include "NowPlaying.h"

namespace spotify {

enum class Command : uint8_t {
    Next,
    Previous,
    TogglePlayback,
    RefreshDevices,  // re-read what Spotify can currently see
    SelectDevice,    // move playback to a chosen device
};

constexpr uint8_t kMaxDevices = 8;

struct Device {
    char id[64] = {};
    char name[64] = {};
    char type[24] = {};  // "Computer", "Smartphone", "Speaker", ...
    bool active = false;
    bool restricted = false;  // listed, but rejects Web API control
};

struct DeviceList {
    uint8_t count = 0;
    bool loading = false;
    uint32_t generation = 0;  // bumped on every successful refresh
    char preferred_id[64] = {};
    Device items[kMaxDevices];
};

// Starts the network task. Safe to call once, from setup().
bool begin();

// Copies the current state under the state mutex.
void snapshot(app::NowPlaying &out);
void deviceSnapshot(DeviceList &out);

// Queues a transport command. Returns false only if the queue is full.
bool send(Command command);

// Queues a move of playback to `device_id`, which also becomes the device the
// board wakes automatically from then on.
bool selectDevice(const char *device_id);

}  // namespace spotify

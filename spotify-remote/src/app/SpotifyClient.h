#pragma once
// Everything that talks to Spotify, on its own task pinned to core 0 so a
// 500 ms HTTPS round trip never stalls touch or drawing on core 1.

#include "NowPlaying.h"

namespace spotify {

enum class Command : uint8_t { Next, Previous, TogglePlayback };

// Starts the network task. Safe to call once, from setup().
bool begin();

// Copies the current state under the state mutex.
void snapshot(app::NowPlaying &out);

// Queues a transport command. Returns false only if the queue is full.
bool send(Command command);

}  // namespace spotify

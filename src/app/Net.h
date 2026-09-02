#pragma once

#include <WiFiClientSecure.h>

namespace net {

// Attaches the ESP-IDF root certificate bundle that is already compiled into the
// Arduino libs, so every connection is properly validated. No setInsecure().
void secure(WiFiClientSecure &client);

// Connects (or reconnects) to the configured network. Non-blocking beyond a
// short wait; returns the current connection state.
bool ensureWifi();

}  // namespace net

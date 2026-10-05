// Copy to src/Secrets.h and fill in, or let tools/spotify_auth.py --write-secrets
// generate it for you. src/Secrets.h is git-ignored.
#pragma once

#define WIFI_SSID              "your-2.4GHz-network"
#define WIFI_PASSWORD          "your-password"

// From https://developer.spotify.com/dashboard -- the client ID is not a secret
// under PKCE, and no client secret is needed (or wanted) on the device.
#define SPOTIFY_CLIENT_ID      "0000000000000000000000000000000"

// Written locally by tools/local_setup.py or spotify_auth.py --write-secrets.
// Used only to seed NVS on first boot; after that the device keeps its own
// rotated copy.
#define SPOTIFY_REFRESH_TOKEN  "paste-the-refresh-token-here"
#define SPOTIFY_AUTH_VERSION   "example-config"

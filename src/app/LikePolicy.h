#pragma once
#include <cstring>
#include <string>

namespace spotify {
// Accept only regular Spotify track IDs. Never save episodes or local-file URIs.
inline bool isSaveableTrack(const char *uri) {
    if (!uri || std::strncmp(uri, "spotify:track:", 14) != 0 || std::strlen(uri) != 36) return false;
    for (const char *p = uri + 14; *p; ++p) {
        if (!((*p >= '0' && *p <= '9') || (*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z'))) return false;
    }
    return true;
}
inline std::string saveTrackUrl(const char *uri) {
    return isSaveableTrack(uri)
        ? std::string("https://api.spotify.com/v1/me/library?uris=spotify%3Atrack%3A") + (uri + 14)
        : std::string();
}
inline bool saveConfirmed(int code) { return code >= 200 && code < 300; }
} // namespace spotify

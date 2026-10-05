#pragma once
#include <cstring>
namespace spotify {
inline bool isPlaylistUri(const char *uri) {
    if (!uri || std::strncmp(uri,"spotify:playlist:",17) != 0 || std::strlen(uri) != 39) return false;
    for (const char *p = uri + 17; *p; ++p) {
        if (!((*p >= '0' && *p <= '9') || (*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z'))) return false;
    }
    return true;
}
}

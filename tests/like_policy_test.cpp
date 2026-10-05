#include "LikePolicy.h"
#include <cassert>
#include <iostream>
int main() {
    const char *track = "spotify:track:7a3LWj5xSFhFRYmztS8wgK";
    assert(spotify::isSaveableTrack(track));
    assert(spotify::saveTrackUrl(track) == "https://api.spotify.com/v1/me/library?uris=spotify%3Atrack%3A7a3LWj5xSFhFRYmztS8wgK");
    for (const char *invalid : {"", "spotify:episode:7a3LWj5xSFhFRYmztS8wgK", "spotify:local:artist:title", "spotify:track:short", "spotify:track:7a3LWj5xSFhFRYmztS8wg&"}) {
        assert(!spotify::isSaveableTrack(invalid));
        assert(spotify::saveTrackUrl(invalid).empty());
    }
    assert(!spotify::isSaveableTrack(nullptr));
    assert(spotify::saveConfirmed(200));
    assert(spotify::saveConfirmed(204));
    for (int code : {-1, 0, 199, 300, 400, 401, 403, 429, 500}) assert(!spotify::saveConfirmed(code));
    std::cout << "Like policy: track targeting, encoding, unsupported items, and failed saves passed\n";
}

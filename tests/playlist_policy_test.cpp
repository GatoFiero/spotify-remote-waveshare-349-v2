#include <cassert>
#include <iostream>
#include "PlaylistPolicy.h"
int main() {
    assert(spotify::isPlaylistUri("spotify:playlist:37i9dQZF1DXcBWIGoYBM5M"));
    assert(!spotify::isPlaylistUri("spotify:track:37i9dQZF1DXcBWIGoYBM5M"));
    assert(!spotify::isPlaylistUri("spotify:playlist:short"));
    assert(!spotify::isPlaylistUri("spotify:playlist:37i9dQZF1DXcBWIGoYBM5!"));
    assert(!spotify::isPlaylistUri(nullptr));
    std::cout << "Valid playlist targeting and malformed URI rejection passed\n";
}

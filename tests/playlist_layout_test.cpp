#include <cassert>
#include <iostream>
#include "PlaylistLayout.h"
int main() {
    for (int row=0;row<4;++row) {
        assert(ui::playlistShortcutAt(9,160,52+row*26)==row*2);
        assert(ui::playlistShortcutAt(9,470,52+row*26)==row*2+1);
        assert(ui::playlistShortcutAt(9,320,52+row*26)==-1);
    }
    assert(ui::playlistShortcutAt(9,160,64)==-1);
    assert(ui::playlistShortcutAt(9,160,143)==-1);
    assert(ui::playlistShortcutAt(9,160,159)==8);
    assert(ui::playlistShortcutAt(9,470,159)==8);
    assert(ui::playlistShortcutAt(9,320,159)==8);
    assert(ui::playlistShortcutAt(8,320,159)==-1);
    assert(ui::playlistShortcutAt(9,610,20)==-2);
    assert(ui::playlistShortcutAt(9,560,35)==-2);
    assert(ui::playlistShortcutAt(9,560,39)==-1);
    assert(ui::playlistShortcutAt(0,160,55)==-1);
    std::cout << "All nine targets, row/column gaps, close control, and empty list passed\n";
}

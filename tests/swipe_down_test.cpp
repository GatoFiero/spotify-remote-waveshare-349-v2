#include <cassert>
#include <iostream>
#include "SwipeDown.h"
int main() {
    ui::SwipeDown s;
    assert(!s.update(true,320,10));
    assert(!s.update(true,325,40));
    assert(s.update(true,330,65));
    assert(!s.startedOnLeft());
    assert(!s.update(true,330,100));
    assert(!s.update(false,330,100));
    assert(!s.update(true,320,70));
    assert(!s.update(true,320,150));
    s.update(false,320,150);
    assert(!s.update(true,100,10));
    assert(!s.update(true,250,80));
    s.update(false,250,80);
    assert(!s.update(true,100,20));
    assert(!s.update(false,100,20));
    assert(s.startedOnLeft());
    using A = ui::QuickAction;
    assert(ui::quickMenuAt(true,60,105)==A::VolumeDown);
    assert(ui::quickMenuAt(true,160,105)==A::VolumeUp);
    assert(ui::quickMenuAt(true,315,90)==A::Shuffle);
    assert(ui::quickMenuAt(true,517,90)==A::Repeat);
    assert(ui::quickMenuAt(false,113,90)==A::Device);
    assert(ui::quickMenuAt(false,260,105)==A::BrightDown);
    assert(ui::quickMenuAt(false,360,105)==A::BrightUp);
    assert(ui::quickMenuAt(false,517,90)==A::Shutdown);
    assert(ui::quickMenuAt(true,610,20)==A::Close);
    assert(ui::quickMenuAt(false,610,20)==A::Close);
    assert(ui::quickMenuAt(true,560,35)==A::Close);
    assert(ui::quickMenuAt(false,560,39)==A::None);
    assert(ui::quickMenuAt(true,215,90)==A::None);
    ui::SwipeUp up;
    assert(!up.update(true,300,165));
    assert(up.update(true,310,100));
    assert(!up.update(true,310,80));
    up.update(false,310,80);
    assert(!up.update(true,300,100));
    assert(!up.update(true,300,40));
    std::cout << "Left/right start selection, bottom swipe, tap exclusion, and menu isolation passed\n";
}

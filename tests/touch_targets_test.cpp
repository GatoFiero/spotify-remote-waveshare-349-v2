#include "TouchTargets.h"
#include <cassert>
#include <iostream>
int main() {
    for (int y : {24, 76, 104, 109, 120, 128}) {
        assert(ui::playerButtonAt(512,y)==ui::Button::Previous);
        assert(ui::playerButtonAt(560,y)==ui::Button::PlayPause);
        assert(ui::playerButtonAt(608,y)==ui::Button::Next);
    }
    assert(ui::playerButtonAt(537,76)==ui::Button::Previous);
    assert(ui::playerButtonAt(538,76)==ui::Button::PlayPause);
    assert(ui::playerButtonAt(585,76)==ui::Button::PlayPause);
    assert(ui::playerButtonAt(586,76)==ui::Button::Next);
    assert(ui::playerButtonAt(515,150)==ui::Button::Like);
    assert(ui::playerButtonAt(515,129)==ui::Button::None);
    assert(ui::playerButtonAt(560,150)==ui::Button::None);
    assert(ui::playerButtonAt(608,150)==ui::Button::None);
    assert(ui::playerButtonAt(400,76)==ui::Button::None);
    std::cout << "Transport centers, lower edges, distinct boundaries, and heart isolation passed\n";
}

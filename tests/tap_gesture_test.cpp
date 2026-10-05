#include <cassert>
#include <iostream>
#include "TapGesture.h"
int main() {
    ui::TapGesture tap;
    using P = ui::TapPhase;
    assert(tap.update(false,0,0)==P::Idle);
    assert(tap.update(true,100,50)==P::Start);
    assert(tap.update(true,110,62)==P::Hold && tap.accepted());
    assert(tap.update(false,110,62)==P::Release && tap.accepted());
    assert(tap.update(false,110,62)==P::Idle);
    assert(tap.update(true,100,50)==P::Start);
    tap.update(true,100,100);
    assert(!tap.accepted());
    tap.update(true,100,50);
    assert(!tap.accepted());
    assert(tap.update(false,100,50)==P::Release && !tap.accepted());
    assert(tap.update(true,100,50)==P::Start && tap.accepted());
    std::cout << "Small tap drift, single release, drag cancellation, and next-tap recovery passed\n";
}

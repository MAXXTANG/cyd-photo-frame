#include "touch_gesture.h"
#include <cassert>
#include <cstdio>
int main() {
    TouchGesture g;
    assert(g.update(true,10)==TouchAction::None);
    assert(g.update(false,80)==TouchAction::None);
    assert(g.update(false,170)==TouchAction::Next);
    assert(g.update(false,200)==TouchAction::None);
    assert(g.update(true,300)==TouchAction::None);
    assert(g.update(true,1099)==TouchAction::None);
    assert(g.update(true,1100)==TouchAction::TogglePlayback);
    assert(g.update(true,2000)==TouchAction::None);
    assert(g.update(false,2200)==TouchAction::None);
    // A brief pressure dropout must not become a second tap.
    assert(g.update(true,2300)==TouchAction::None);
    assert(g.update(false,2350)==TouchAction::None);
    assert(g.update(true,2400)==TouchAction::None);
    assert(g.update(false,2600)==TouchAction::Next);
    // A released short tap near the threshold must not become a long press.
    assert(g.update(true,3000)==TouchAction::None);
    assert(g.update(false,3790)==TouchAction::Next);
    assert(g.update(false,3850)==TouchAction::None);
    TouchGesture wrap;
    assert(wrap.update(true,0xffffff00u)==TouchAction::None);
    assert(wrap.update(true,0x220u)==TouchAction::TogglePlayback);
    puts("PASS: short tap, long hold once, release, contact noise and timer wrap");
}

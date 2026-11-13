#pragma once
#include <stdint.h>

enum class TouchAction { None, Next, TogglePlayback };
class TouchGesture {
    bool active=false,held=false;
    uint32_t started=0,lastContact=0;
public:
    TouchAction update(bool down,uint32_t now) {
        if(down) {
            if(!active){active=true;held=false;started=now;}
            lastContact=now;
            if(!held && uint32_t(now-started)>=800){held=true;return TouchAction::TogglePlayback;}
        } else if(active && uint32_t(now-lastContact)>=150) {
            active=false;
            if(!held)return TouchAction::Next;
        }
        return TouchAction::None;
    }
};

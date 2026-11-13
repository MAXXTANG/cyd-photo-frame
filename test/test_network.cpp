#include "network_config.h"
#include <cassert>
#include <cstdio>
#include <string>
int main() {
    assert(validNetwork("Home 2.4G","12345678"));
    assert(validNetwork("Open network",""));
    assert(!validNetwork("","12345678"));
    assert(!validNetwork(std::string(33,'s').c_str(),"12345678"));
    assert(validNetwork(std::string(32,'s').c_str(),"12345678"));
    assert(!validNetwork("Home","short"));
    assert(validNetwork("Home",std::string(63,'p').c_str()));
    assert(validNetwork("Home",std::string(64,'a').c_str()));
    assert(!validNetwork("Home",std::string(64,'z').c_str()));
    assert(!validNetwork("Home",std::string(65,'a').c_str()));
    ConnectSchedule timer;
    assert(!timer.due(0));timer.request(1000,750);
    assert(!timer.due(1749));assert(timer.due(1750));
    timer.attempted(1750);assert(!timer.due(31749));assert(timer.due(31750));
    timer.request(0xffffff00,750);assert(!timer.due(493));assert(timer.due(494));
    timer.disable();assert(!timer.due(100000));
    puts("PASS: Wi-Fi credential bounds, open network, PSK and reconnect schedule/wrap");
}

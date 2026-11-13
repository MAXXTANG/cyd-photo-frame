#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>

struct HomeNetwork {
    char ssid[33]{};
    char password[65]{};
};

inline bool validNetwork(const char *ssid,const char *password) {
    size_t s=strlen(ssid),p=strlen(password);
    if(!s || s>32 || p>64)return false;
    if(p==0)return true;
    if(p<8)return false;
    if(p==64)for(size_t i=0;i<p;i++) {
        char c=password[i];
        if(!((c>='0'&&c<='9')||(c>='a'&&c<='f')||(c>='A'&&c<='F')))return false;
    }
    return true;
}

class ConnectSchedule {
    bool enabled=false;
    uint32_t next=0;
public:
    void request(uint32_t now,uint32_t delay=0){enabled=true;next=now+delay;}
    void disable(){enabled=false;}
    bool due(uint32_t now)const{return enabled && int32_t(now-next)>=0;}
    void attempted(uint32_t now){request(now,30000);}
};

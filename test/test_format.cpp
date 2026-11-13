#include "photo_format.h"
#include <cassert>
#include <vector>
#include <cstdio>
int main() {
    using namespace photo;
    assert((crcUpdate(0xffffffffu,(const uint8_t*)"123456789",9)^0xffffffffu)==0xcbf43926u);
    std::vector<uint8_t> file(FILE_SIZE,0);
    memcpy(file.data(),"CYD1",4);file[4]=240;file[6]=64;file[7]=1;
    file[8]=0;file[9]=0x58;file[10]=2;
    Header h;assert(decodeHeader(file.data(),file.size(),h));assert(h.w==240&&h.h==320);
    assert(!decodeHeader(file.data(),file.size()-1,h));
    file[4]=0;assert(!decodeHeader(file.data(),file.size(),h));file[4]=240;
    file[8]=1;assert(!decodeHeader(file.data(),file.size(),h));file[8]=0;
    file[0]='X';assert(!decodeHeader(file.data(),file.size(),h));
    auto f=fit(320,240,240,320);assert(f.w==240&&f.h==180&&f.x==0&&f.y==70);
    f=fit(240,320,320,240);assert(f.w==180&&f.h==240&&f.x==70&&f.y==0);
    uint8_t order[]={2,0,1};assert(validOrder(order,3,7));
    order[1]=2;assert(!validOrder(order,3,7));order[1]=12;assert(!validOrder(order,3,7));
    order[1]=0;assert(!validOrder(order,2,7));assert(validOrder(nullptr,0,0));
    for(int a=0;a<4;a++)for(int b=0;b<4;b++) {
        auto x=fit(a%2?320:240,a%2?240:320,b%2?320:240,b%2?240:320);
        assert(x.w>0&&x.h>0&&x.x>=0&&x.y>=0);
    }
    ContentBounds legacy(240,320);
    for(int y=70;y<250;y++)for(int x=0;x<240;x++)legacy.pixel(y*240+x,0xffff);
    auto bounds=legacy.result();assert(bounds.x==0&&bounds.y==70&&bounds.w==240&&bounds.h==180);
    assert(displayRotation(4,bounds.w,bounds.h)==1);
    assert(displayRotation(4,240,320)==0);assert(displayRotation(4,240,240)==0);
    for(int mode=0;mode<4;mode++)assert(displayRotation(mode,320,240)==mode);
    auto full=fit(bounds.w,bounds.h,320,240);assert(full.w==320&&full.h==240&&full.x==0&&full.y==0);
    full=fit(240,320,320,240);assert(full.w==180&&full.h==240&&full.x==70);
    full=fit(320,240,240,320);assert(full.w==240&&full.h==180&&full.y==70);
    ContentBounds black(320,240);bounds=black.result();assert(bounds.w==320&&bounds.h==240);
    full=fit(1,320,320,240);assert(full.w>=1&&full.h>=1);
    // The full source rectangle is used; no zoom/crop window is introduced.
    for(int w:{1,17,240,320})for(int h:{1,23,240,320})for(int dw:{240,320}) {
        int dh=dw==240?320:240;auto c=fit(w,h,dw,dh);
        assert(c.w>=1&&c.h>=1&&c.x>=0&&c.y>=0&&c.x+c.w<=dw&&c.y+c.h<=dh);
        assert((c.w-1)*w/c.w<w&&(c.h-1)*h/c.h<h);
    }
    puts("PASS: format/CRC, playlist, automatic orientation, legacy black bars and full-image fit bounds");
}

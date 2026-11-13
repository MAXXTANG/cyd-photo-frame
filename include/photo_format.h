#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>

namespace photo {
constexpr int MAX_IMAGES = 12;
constexpr size_t HEADER_SIZE = 16;
constexpr size_t PIXEL_BYTES = 240 * 320 * 2;
constexpr size_t FILE_SIZE = HEADER_SIZE + PIXEL_BYTES;
inline uint16_t u16(const uint8_t *p) { return p[0] | uint16_t(p[1]) << 8; }
inline uint32_t u32(const uint8_t *p) { return uint32_t(u16(p)) | uint32_t(u16(p+2)) << 16; }
inline uint32_t crcUpdate(uint32_t crc, const uint8_t *p, size_t n) {
    while (n--) { crc ^= *p++; for (int b=0;b<8;b++) crc = (crc >> 1) ^ (0xedb88320u & (0u-(crc&1))); }
    return crc;
}
struct Header { uint16_t w=0,h=0; uint32_t crc=0; };
inline bool decodeHeader(const uint8_t *p, size_t size, Header &h) {
    if (size != FILE_SIZE || memcmp(p,"CYD1",4)) return false;
    h.w=u16(p+4);h.h=u16(p+6);h.crc=u32(p+12);
    return ((h.w==240 && h.h==320)||(h.w==320 && h.h==240)) && u32(p+8)==PIXEL_BYTES;
}
struct Fit { int w,h,x,y; };
// Ignore only completely black outer rows/columns left by legacy letterboxing.
// An all-black image keeps its full bounds.
struct Bounds {
    int x,y,w,h;
};
struct ContentBounds {
    int width,height,left,top,right=-1,bottom=-1;
    ContentBounds(int w,int h):width(w),height(h),left(w),top(h){}
    void pixel(size_t index,uint16_t color) {
        if(!color)return;
        int x=index%width,y=index/width;
        if(x<left)left=x;if(x>right)right=x;
        if(y<top)top=y;if(y>bottom)bottom=y;
    }
    Bounds result()const {
        return right<left ? Bounds{0,0,width,height} : Bounds{left,top,right-left+1,bottom-top+1};
    }
};
inline uint8_t displayRotation(uint8_t mode,int width,int height) {
    return mode==4 ? (width>height ? 1:0) : mode%4;
}
inline Fit fit(int sw,int sh,int dw,int dh) {
    int w=dw, h=sh*dw/sw;
    if(h>dh){h=dh;w=sw*dh/sh;}
    if(w<1)w=1;if(h<1)h=1;
    return {w,h,(dw-w)/2,(dh-h)/2};
}
inline bool validOrder(const uint8_t *ids, size_t n, uint16_t mask) {
    uint16_t seen=0;
    for(size_t i=0;i<n;i++) { if(ids[i]>=MAX_IMAGES || (seen&(1u<<ids[i])))return false;seen|=1u<<ids[i]; }
    return seen==mask;
}
}

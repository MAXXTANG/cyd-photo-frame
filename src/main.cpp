#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <LittleFS.h>
#include <Preferences.h>
#include <TFT_eSPI.h>
#include <SPI.h>
#include <ESPmDNS.h>
#include "photo_format.h"
#include "touch_gesture.h"
#include "network_config.h"
#include "web_page.h"

TFT_eSPI lcd;
SPIClass touchSPI(VSPI);
WebServer server(80);
Preferences prefs;
bool mounted=false, playing=true, info=false, dirty=true;
uint8_t rotation=4, brightness=200, order[photo::MAX_IMAGES];
struct ImageInfo { photo::Header header;photo::Bounds bounds{0,0,240,320}; };
ImageInfo imageInfo[photo::MAX_IMAGES];
int count=0, current=0;
uint32_t intervalMs=10000, lastSlide=0, infoUntil=0;
String ssid, password;
String hostname;
HomeNetwork homeNetwork;
ConnectSchedule connectSchedule;
bool homeConnected=false,mdnsStarted=false,networkChangePending=false;
uint32_t connectStarted=0,mdnsRetryAt=0;
File uploadFile;
size_t received=0;
int uploadSlot=-1, uploadParts=0;
bool uploadReady=false;
String uploadError;
uint16_t rowSource[320], rowOutput[320];
TouchGesture gesture;
const char *notice=nullptr;
uint32_t noticeUntil=0;

String jsonString(const String &value) {
    String result="\"";
    for(size_t i=0;i<value.length();i++) {
        uint8_t c=value[i];
        if(c=='\"'||c=='\\'){result+='\\';result+=char(c);}
        else if(c<32){char b[7];snprintf(b,sizeof(b),"\\u%04x",c);result+=b;}
        else result+=char(c);
    }
    return result+'\"';
}
bool authenticated() {
    if(server.authenticate("admin",password.c_str()))return true;
    server.requestAuthentication(BASIC_AUTH,"CYD Photo Frame","Authentication required");
    return false;
}
void networkLoop() {
    uint32_t now=millis();
    if(networkChangePending && connectSchedule.due(now)) {
        networkChangePending=false;
        WiFi.disconnect(false,false);
        WiFi.begin(homeNetwork.ssid,homeNetwork.password);
        connectStarted=now;connectSchedule.attempted(now);
    }
    bool connected=WiFi.status()==WL_CONNECTED;
    if(connected!=homeConnected) {
        homeConnected=connected;dirty=true;
        if(!connected && mdnsStarted){MDNS.end();mdnsStarted=false;mdnsRetryAt=now;}
    }
    if(connected) {
        if(!mdnsStarted && int32_t(now-mdnsRetryAt)>=0) {
            if(MDNS.begin(hostname.c_str())){MDNS.addService("http","tcp",80);mdnsStarted=true;}
            mdnsRetryAt=now+5000;
        }
        return;
    }
    if(homeNetwork.ssid[0] && connectSchedule.due(now)) {
        WiFi.disconnect(false,false);
        WiFi.begin(homeNetwork.ssid,homeNetwork.password);
        connectStarted=now;connectSchedule.attempted(now);
    }
}
void networkStatus() {
    if(!authenticated())return;
    bool connected=WiFi.status()==WL_CONNECTED && !networkChangePending;
    String phase=!homeNetwork.ssid[0]?"not_configured":connected?"connected":
        (millis()-connectStarted>=15000?"retrying":"connecting");
    String json="{\"configured\":"+String(homeNetwork.ssid[0]?"true":"false")+
        ",\"connected\":"+(connected?"true":"false")+",\"state\":"+jsonString(phase)+
        ",\"ssid\":"+jsonString(homeNetwork.ssid)+",\"ip\":"+jsonString(connected?WiFi.localIP().toString():"")+
        ",\"hostname\":"+jsonString(hostname+".local")+",\"ap_ssid\":"+jsonString(ssid)+
        ",\"ap_ip\":"+jsonString(WiFi.softAPIP().toString())+"}";
    server.sendHeader("Cache-Control","no-store");server.send(200,"application/json",json);
}

String pathFor(int id) { return "/p" + String(id) + ".cyd"; }
void saveOrder() { prefs.putBytes("order",order,count); }
void saveSettings() {
    prefs.putBool("play",playing);prefs.putUInt("interval",intervalMs);
    prefs.putUChar("layout",rotation);prefs.putUChar("brightness",brightness);
}
bool headerFor(File &f, photo::Header &h) {
    uint8_t b[16];return f && f.seek(0) && f.read(b,16)==16 && photo::decodeHeader(b,f.size(),h);
}
bool verify(File &f,ImageInfo *metadata=nullptr) {
    photo::Header h;if(!headerFor(f,h))return false;
    uint8_t buf[512];uint32_t crc=0xffffffffu;size_t pixel=0;
    photo::ContentBounds bounds(h.w,h.h);
    while(f.available()){
        size_t n=f.read(buf,sizeof(buf));if(!n || n%2)return false;
        crc=photo::crcUpdate(crc,buf,n);
        for(size_t i=0;i<n;i+=2)bounds.pixel(pixel++,photo::u16(buf+i));
        yield();
    }
    if((crc^0xffffffffu)!=h.crc)return false;
    if(metadata){metadata->header=h;metadata->bounds=bounds.result();}
    return true;
}
void scanImages() {
    uint16_t mask=0;
    for(int i=0;i<photo::MAX_IMAGES;i++) {
        File f=LittleFS.open(pathFor(i),"r");if(verify(f,&imageInfo[i]))mask|=1u<<i;
    }
    uint8_t saved[photo::MAX_IMAGES];size_t n=prefs.getBytesLength("order");
    if(n>sizeof(saved))n=0;
    if(n)prefs.getBytes("order",saved,n);
    count=0;
    for(size_t i=0;i<n;i++)if(saved[i]<photo::MAX_IMAGES && (mask&(1u<<saved[i]))) {
        order[count++]=saved[i];mask &= ~(1u<<saved[i]);
    }
    for(int i=0;i<photo::MAX_IMAGES;i++)if(mask&(1u<<i))order[count++]=i;
}
void drawInfo() {
    lcd.setRotation(rotation==4?0:rotation);
    lcd.fillScreen(TFT_BLACK);lcd.setTextColor(TFT_WHITE,TFT_BLACK);lcd.setTextFont(2);
    lcd.setCursor(10,8);lcd.println("CYD Photo Frame 0.2.0");
    lcd.setCursor(10,34);lcd.println(!mounted?"Storage unavailable!":homeConnected?"Home Wi-Fi: connected":"Home Wi-Fi: not connected");
    lcd.setCursor(10,54);lcd.println(homeConnected?"http://"+WiFi.localIP().toString():"Setup with hotspot below");
    lcd.setCursor(10,76);lcd.println(hostname+".local");
    lcd.setCursor(10,100);lcd.println("AP: "+ssid);
    lcd.setCursor(10,122);lcd.println("http://192.168.4.1");
    lcd.setCursor(10,146);lcd.println("Login: admin");
    lcd.setCursor(10,166);lcd.println("Password: " + password);
    lcd.setCursor(10,192);lcd.println("Tap: next / Hold: auto");
    lcd.setCursor(10,214);lcd.println("BOOT: info / "+String(count)+" images");
}
void render() {
    dirty=false;
    if(info || !count || !mounted){drawInfo();return;}
    File f=LittleFS.open(pathFor(order[current]),"r");photo::Header h;
    if(!headerFor(f,h)){lcd.fillScreen(TFT_RED);return;}
    auto bounds=imageInfo[order[current]].bounds;
    lcd.setRotation(photo::displayRotation(rotation,bounds.w,bounds.h));
    auto fit=photo::fit(bounds.w,bounds.h,lcd.width(),lcd.height());
    lcd.fillScreen(TFT_BLACK);lcd.setSwapBytes(true);
    int previous=-1;
    for(int y=0;y<fit.h;y++) {
        int sy=bounds.y+y*bounds.h/fit.h;
        if(sy!=previous) {
            if(!f.seek(16+size_t(sy)*h.w*2) || f.read((uint8_t*)rowSource,h.w*2)!=size_t(h.w)*2)return;
            previous=sy;
        }
        for(int x=0;x<fit.w;x++)rowOutput[x]=rowSource[bounds.x+x*bounds.w/fit.w];
        lcd.pushImage(fit.x,fit.y+y,fit.w,1,rowOutput);yield();
    }
    if(notice && int32_t(noticeUntil-millis())>0) {
        lcd.fillRoundRect(6,lcd.height()-29,lcd.width()-12,24,5,TFT_DARKGREY);
        lcd.setTextFont(2);lcd.setTextColor(TFT_WHITE,TFT_DARKGREY);
        lcd.drawCentreString(notice,lcd.width()/2,lcd.height()-26,2);
    }
}
void advance(int step=1) {
    info=false;if(count)current=(current+step+count)%count;
    dirty=true;lastSlide=millis();
}
bool allowed(bool needsStorage=true) {
    if(!authenticated())return false;
    if(server.header("X-Photo-Request")!="1") {server.send(403,"text/plain","Open the frame's upload page first.");return false;}
    if(needsStorage && !mounted){server.send(503,"text/plain","Storage is unavailable; reinstall filesystem over USB.");return false;}
    return true;
}
void status() {
    if(!authenticated())return;
    String json="{\"version\":\"0.2.0\",\"count\":"+String(count)+",\"playing\":"+(playing?"true":"false")+
        ",\"seconds\":"+String(intervalMs/1000)+",\"rotation\":"+String(rotation)+",\"brightness\":"+String(brightness)+
        ",\"current\":"+String(count?order[current]:-1)+",\"width\":"+String(lcd.width())+",\"height\":"+String(lcd.height())+
        ",\"storage\":"+(mounted?"true":"false")+",\"free\":"+String(mounted?LittleFS.totalBytes()-LittleFS.usedBytes():0)+",\"images\":[";
    for(int i=0;i<count;i++){if(i)json+=",";json+=String(order[i]);}
    json+="]}";server.send(200,"application/json",json);
}
void finishUpload() {
    if(!allowed())return;
    if(!uploadReady || uploadError.length() || uploadParts!=1) {
        LittleFS.remove("/incoming.tmp");server.send(400,"text/plain",uploadError.length()?uploadError:"Incomplete upload");return;
    }
    ImageInfo metadata;
    File f=LittleFS.open("/incoming.tmp","r");bool ok=verify(f,&metadata);f.close();
    if(!ok){LittleFS.remove("/incoming.tmp");server.send(400,"text/plain","Image length, dimensions or checksum failed");return;}
    if(!LittleFS.rename("/incoming.tmp",pathFor(uploadSlot))) {server.send(500,"text/plain","Could not save image");return;}
    imageInfo[uploadSlot]=metadata;order[count++]=uploadSlot;saveOrder();if(count==1){current=0;info=false;}dirty=true;lastSlide=millis();
    server.send(200,"text/plain","Saved");uploadReady=false;
}
void uploadChunk() {
    HTTPUpload &u=server.upload();
    if(u.status==UPLOAD_FILE_START) {
        // One request may contain only one file; never silently accept a second.
        if(uploadParts++) {uploadError="Upload one file per request";if(uploadFile)uploadFile.close();return;}
        uploadReady=false;received=0;uploadError="";uploadSlot=-1;
        if(!server.authenticate("admin",password.c_str()) || server.header("X-Photo-Request")!="1" || !mounted){uploadError="Storage or request unavailable";return;}
        if(count>=photo::MAX_IMAGES){uploadError="Album is full (12 images)";return;}
        for(int i=0;i<photo::MAX_IMAGES;i++)if(!LittleFS.exists(pathFor(i))){uploadSlot=i;break;}
        if(uploadSlot<0){uploadError="No free slot; remove invalid files over USB";return;}
        LittleFS.remove("/incoming.tmp");
        if(LittleFS.totalBytes()-LittleFS.usedBytes()<photo::FILE_SIZE+32768){uploadError="Storage is full";return;}
        uploadFile=LittleFS.open("/incoming.tmp","w");if(!uploadFile)uploadError="Cannot create file";
    } else if(u.status==UPLOAD_FILE_WRITE && !uploadError.length()) {
        if(received+u.currentSize>photo::FILE_SIZE){uploadError="Image exceeds size limit";uploadFile.close();return;}
        if(uploadFile.write(u.buf,u.currentSize)!=u.currentSize){uploadError="Storage write failed";uploadFile.close();return;}
        received+=u.currentSize;
    } else if(u.status==UPLOAD_FILE_END) {
        if(uploadFile)uploadFile.close();uploadReady=received==photo::FILE_SIZE && !uploadError.length();
    } else if(u.status==UPLOAD_FILE_ABORTED) {
        if(uploadFile)uploadFile.close();LittleFS.remove("/incoming.tmp");uploadReady=false;uploadParts=0;
    }
}
bool intArg(const char *key,int lo,int hi,int &value) {
    String s=server.arg(key);if(!s.length())return false;
    for(size_t i=0;i<s.length();i++)if(s[i]<'0'||s[i]>'9')return false;
    if(s.length()>6)return false;value=s.toInt();return value>=lo&&value<=hi;
}
void routes() {
    const char *headers[]={"X-Photo-Request"};server.collectHeaders(headers,1);
    server.on("/",HTTP_GET,[]{if(!authenticated())return;server.sendHeader("Cache-Control","no-store");server.send_P(200,"text/html; charset=utf-8",WEB_PAGE);});
    server.on("/api/status",HTTP_GET,status);
    server.on("/api/network",HTTP_GET,networkStatus);
    server.on("/api/network",HTTP_POST,[]{
        if(!allowed(false))return;
        String name=server.arg("ssid"),key=server.arg("password");
        if(name.length()!=strlen(name.c_str()) || key.length()!=strlen(key.c_str()) || !validNetwork(name.c_str(),key.c_str())) {
            server.send(400,"text/plain","SSID must be 1-32 bytes; password empty or 8-63 bytes (64 hex digits also accepted)");return;
        }
        HomeNetwork next;memcpy(next.ssid,name.c_str(),name.length());memcpy(next.password,key.c_str(),key.length());
        if(prefs.putBytes("homewifi",&next,sizeof(next))!=sizeof(next)){server.send(500,"text/plain","Could not save network");return;}
        homeNetwork=next;networkChangePending=true;connectStarted=millis();connectSchedule.request(millis(),750);
        server.send(202,"text/plain","Saved; connecting. The recovery hotspot remains available.");
    });
    server.on("/api/network/forget",HTTP_POST,[]{
        if(!allowed(false))return;
        if(prefs.isKey("homewifi") && !prefs.remove("homewifi")){server.send(500);return;}
        homeNetwork=HomeNetwork{};networkChangePending=false;connectSchedule.disable();
        server.send(200,"text/plain","Forgot home network. Use the recovery hotspot.");
        WiFi.disconnect(false,false);
    });
    server.on("/api/upload",HTTP_POST,[]{finishUpload();uploadParts=0;},uploadChunk);
    server.on("/api/image",HTTP_GET,[]{
        if(!authenticated())return;
        int id;if(!intArg("id",0,11,id)){server.send(400);return;}
        File f=LittleFS.open(pathFor(id),"r");if(!f){server.send(404);return;}
        server.sendHeader("Cache-Control","no-store");server.streamFile(f,"application/octet-stream");
    });
    server.on("/api/delete",HTTP_POST,[]{
        if(!allowed())return;int id;if(!intArg("id",0,11,id)){server.send(400);return;}
        int at=-1;for(int i=0;i<count;i++)if(order[i]==id)at=i;
        if(at<0){server.send(404);return;}
        if(!LittleFS.remove(pathFor(id))){server.send(500);return;}
        for(int i=at;i<count-1;i++)order[i]=order[i+1];count--;current=0;saveOrder();dirty=true;lastSlide=millis();server.send(200);
    });
    server.on("/api/settings",HTTP_POST,[]{
        if(!allowed())return;int seconds,r,b,p;
        if(!intArg("seconds",2,3600,seconds)||!intArg("rotation",0,4,r)||!intArg("brightness",10,255,b)||!intArg("playing",0,1,p)){server.send(400,"text/plain","Invalid settings");return;}
        intervalMs=uint32_t(seconds)*1000;rotation=r;brightness=b;playing=p;
        saveSettings();ledcWrite(0,brightness);dirty=true;lastSlide=millis();server.send(200);
    });
    server.on("/api/control",HTTP_POST,[]{
        if(!allowed())return;String action=server.arg("action");
        if(action=="next")advance();else if(action=="prev")advance(-1);
        else if(action=="show") {int id;if(!intArg("id",0,11,id)){server.send(400);return;}int at=-1;for(int i=0;i<count;i++)if(order[i]==id)at=i;if(at<0){server.send(404);return;}current=at;info=false;dirty=true;lastSlide=millis();}
        else {server.send(400);return;}server.send(200);
    });
    server.on("/api/order",HTTP_POST,[]{
        if(!allowed())return;String s=server.arg("ids");uint8_t ids[12];int n=0;
        if(s.length()>36){server.send(400);return;}
        while(s.length()) {int comma=s.indexOf(',');String item=comma<0?s:s.substring(0,comma);if(!item.length()||item.length()>2||n>=12){server.send(400);return;}
            for(size_t i=0;i<item.length();i++)if(!isDigit(item[i])){server.send(400);return;}
            ids[n++]=item.toInt();if(comma<0)break;s=s.substring(comma+1);if(!s.length()){server.send(400);return;}}
        uint16_t mask=0;for(int i=0;i<count;i++)mask|=1u<<order[i];
        if(n!=count||!photo::validOrder(ids,n,mask)){server.send(400);return;}
        memcpy(order,ids,n);current=0;saveOrder();dirty=true;lastSlide=millis();server.send(200);
    });
    server.onNotFound([]{server.send(404,"text/plain","Not found");});server.begin();
}
bool touched() {
    if(digitalRead(36)!=LOW)return false;
    touchSPI.beginTransaction(SPISettings(2000000,MSBFIRST,SPI_MODE0));digitalWrite(33,LOW);
    auto sample=[](uint8_t c){touchSPI.transfer(c);return (touchSPI.transfer16(0)>>3)&4095;};
    int z1=sample(0xb0),z2=sample(0xc0);sample(0x80);
    digitalWrite(33,HIGH);touchSPI.endTransaction();return z1+4095-z2>400;
}
// USB service path for replacing an existing, known image without touching the
// album or filesystem layout. CRC compare prevents replacing a different image.
void replaceFromSerial() {
    String line=Serial.readStringUntil('\n');int slot=-1;unsigned size=0;unsigned long oldCrc=0;char trailing;
    if(sscanf(line.c_str(),"%d %u %lx %c",&slot,&size,&oldCrc,&trailing)!=3 || !mounted ||
       slot<0 || slot>=photo::MAX_IMAGES || size!=photo::FILE_SIZE ||
       !LittleFS.exists(pathFor(slot)) || imageInfo[slot].header.crc!=uint32_t(oldCrc)) {
        Serial.println("ERR target");return;
    }
    if(LittleFS.totalBytes()-LittleFS.usedBytes()<photo::FILE_SIZE+32768){Serial.println("ERR space");return;}
    File file=LittleFS.open("/incoming.tmp","w");
    if(!file){Serial.println("ERR open");return;}
    Serial.println("READY");
    uint8_t buffer[1024];size_t done=0;uint32_t started=millis();bool ok=true;
    while(done<size) {
        size_t wanted=min(sizeof(buffer),size_t(size-done));
        size_t n=Serial.readBytes(buffer,wanted);
        if(n!=wanted || file.write(buffer,n)!=n || millis()-started>30000){ok=false;break;}
        done+=n;Serial.printf("ACK %u\n",unsigned(done));yield();
    }
    file.close();ImageInfo metadata;
    if(ok){File check=LittleFS.open("/incoming.tmp","r");ok=verify(check,&metadata);}
    if(!ok || !LittleFS.rename("/incoming.tmp",pathFor(slot))) {
        LittleFS.remove("/incoming.tmp");Serial.println("ERR verify");return;
    }
    imageInfo[slot]=metadata;dirty=true;lastSlide=millis();
    Serial.printf("OK %d %u %u\n",slot,metadata.header.w,metadata.header.h);
}
void setup() {
    Serial.setRxBufferSize(2048);Serial.begin(115200);Serial.setTimeout(2000);pinMode(0,INPUT_PULLUP);
    for(int p:{4,16,17}){pinMode(p,OUTPUT);digitalWrite(p,HIGH);}
    prefs.begin("cyd-photo",false);playing=prefs.getBool("play",true);intervalMs=prefs.getUInt("interval",10000);
    if(prefs.getBytesLength("homewifi")==sizeof(homeNetwork)) {
        prefs.getBytes("homewifi",&homeNetwork,sizeof(homeNetwork));
        homeNetwork.ssid[32]=0;homeNetwork.password[64]=0;
        if(!validNetwork(homeNetwork.ssid,homeNetwork.password))homeNetwork=HomeNetwork{};
    }
    if(intervalMs<2000||intervalMs>3600000)intervalMs=10000;
    rotation=prefs.getUChar("layout",4);if(rotation>4)rotation=4;
    brightness=max((uint8_t)10,prefs.getUChar("brightness",200));
    password=prefs.getString("ap-pass","");if(password.length()!=10){char p[11];snprintf(p,sizeof(p),"%010lu",(unsigned long)(esp_random()%1000000000));password=p;prefs.putString("ap-pass",password);}
    lcd.init();lcd.setRotation(rotation==4?0:rotation);ledcSetup(0,5000,8);ledcAttachPin(21,0);ledcWrite(0,brightness);
    pinMode(33,OUTPUT);digitalWrite(33,HIGH);pinMode(36,INPUT);touchSPI.begin(25,39,32,33);
    mounted=LittleFS.begin(false);if(mounted){LittleFS.remove("/incoming.tmp");scanImages();}
    char name[24];snprintf(name,sizeof(name),"CYD-Photo-%04X",(uint16_t)ESP.getEfuseMac());ssid=name;
    hostname=ssid;hostname.toLowerCase();
    WiFi.persistent(false);WiFi.setHostname(hostname.c_str());WiFi.mode(WIFI_AP_STA);
    WiFi.setSleep(false);WiFi.setAutoReconnect(false);
    if(!WiFi.softAP(ssid.c_str(),password.c_str(),1,0,2))Serial.println("AP start failed");
    if(homeNetwork.ssid[0])connectSchedule.request(millis());
    routes();info=true;infoUntil=millis()+15000;lastSlide=millis();
    Serial.printf("CYD Photo Frame 0.2.0 storage=%d count=%d heap=%u\n",mounted,count,ESP.getFreeHeap());
}
void loop() {
    server.handleClient();networkLoop();uint32_t now=millis();
    static uint32_t touchAt=0,buttonAt=0;static bool buttonDown=false,held=false;
    if(now-touchAt>=25) {
        touchAt=now;auto action=gesture.update(touched(),now);
        if(action==TouchAction::Next)advance();
        else if(action==TouchAction::TogglePlayback) {
            playing=!playing;prefs.putBool("play",playing);info=false;dirty=true;lastSlide=now;
            notice=playing?"AUTO PLAY":"MANUAL - tap to change";noticeUntil=now+1500;
        }
    }
    bool button=digitalRead(0)==LOW;
    if(button&&!buttonDown){buttonAt=now;held=false;}
    if(button && !held && now-buttonAt>=1000){held=true;info=!info;infoUntil=now+60000;dirty=true;lastSlide=now;}
    if(!button&&buttonDown&&!held&&now-buttonAt>=30)advance();buttonDown=button;
    if(info&&count&&int32_t(now-infoUntil)>=0){info=false;dirty=true;lastSlide=now;}
    if(!info&&playing&&count>1&&now-lastSlide>=intervalMs)advance();
    if(notice && int32_t(now-noticeUntil)>=0){notice=nullptr;dirty=true;}
    if(dirty)render();
    if(Serial.available()) {
        char command=Serial.read();
        if(command=='J')Serial.printf("{\"count\":%d,\"current\":%d,\"heap\":%u,\"storage\":%d,\"rotation\":%d,\"mode\":%d,\"playing\":%d,\"width\":%d,\"height\":%d}\n",count,current,ESP.getFreeHeap(),mounted,lcd.getRotation(),rotation,playing,lcd.width(),lcd.height());
        else if(command=='N')Serial.printf("{\"configured\":%d,\"connected\":%d,\"ip\":\"%s\",\"hostname\":\"%s.local\",\"ap_clients\":%u}\n",homeNetwork.ssid[0]!=0,WiFi.status()==WL_CONNECTED,WiFi.localIP().toString().c_str(),hostname.c_str(),WiFi.softAPgetStationNum());
        else if(command=='U')replaceFromSerial();
    }
    delay(2);
}

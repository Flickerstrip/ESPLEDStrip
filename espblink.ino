#include <SoftwareSPI.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <WiFiServer.h>
#include <FlashMemory.h>

//BakaNekoGato
//9377689997
char myssid[50] = "Steven's Castle";
char mypassword[50] = "Gizmo3151";

//#if defined(ARDUINO) && ARDUINO >= 100
#include "Arduino.h"
//#else
//#include "WProgram.h"
//#endif


#define SPI_SCK 5
#define SPI_MOSI 14
#define SPI_MISO 16
#define MEM_CS 4
 
int ledPin = 13;
WiFiServer tcpserver(80);
WiFiUDP udp;
WiFiClient tcp;
FlashMemory flash(SPI_SCK,SPI_MOSI,SPI_MISO,MEM_CS);
uint8_t macAddr[WL_MAC_ADDR_LENGTH];
uint16_t stripLength = 150;
uint8_t leds[450];

bool debug = false;
const char* configssid = "esp8266confignetwork";

char buf[1000];
struct PatternMetadata {
  char name[18];
  uint16_t address;
  uint16_t len;
  uint16_t frames;
  uint8_t flags;
  uint8_t fps;
};

const int MAX_PATTERNS = 18;
int patternCount = 0;
PatternMetadata patterns[MAX_PATTERNS];
int selectedPattern;

void dumpPage(uint16_t addr) {
  flash.readBytes(addr,(byte*)buf,256);
  Serial.print("Page: 0x");
  Serial.println(addr,HEX);
  for (int i=0; i<256; i++) {
    if (i % 16 == 0 && i != 0) Serial.println();
    Serial.print("0x");
    if (buf[i] < 0x10) Serial.print("0");
    Serial.print(buf[i],HEX);
    Serial.print(" ");
  }
  Serial.println();
}


void setup() {
  Serial.begin(115200);
  delay(10);
 
  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, LOW);


  /*
  byte r,g,b;
  for (uint16_t i=0; i<stripLength; i++) {
    if (i % 3 == 0) {
      r = 5;
      g = 0;
      b = 0;
    } else if (i % 3 == 1) {
      r = 0;
      g = 5;
      b = 0;
    } else if (i % 3 == 2) {
      r = 0;
      g = 0;
      b = 5;
    }
    leds[i*3] = r;
    leds[i*3+1] = g;
    leds[i*3+2] = b;
  }
  */
  
  WiFi.macAddress(macAddr);
  
  Serial.println("\n");

  delay(1000); //TODO figure out why the memory chip needs some time to start up and add it to the library
/*  
  char foo[] = "foo";
  memcpy(patterns[0].name, foo, 4);
  patternCount = 1;
//  patterns[0].name = foo;
  patterns[0].address = 0x300;
  patterns[0].len = 10*3*2;
  patterns[0].frames = 2;
  patterns[0].flags = 0;
  patterns[0].fps = 1;
  Serial.print("writing: ");
  Serial.println(sizeof(PatternMetadata));
  for (int i=0; i<sizeof(PatternMetadata); i++) {
    Serial.print((((byte *)patterns)[i]),HEX);
    Serial.print(" ");
  }
  Serial.println();
  flash.writeBytes(0x100,(byte *)patterns,sizeof(PatternMetadata));
  dumpPage(0x100);
  
  for (int i=0; i<10; i++) {
    buf[i*3+0] = 10;
    buf[i*3+1] = 10;
    buf[i*3+2] = 10;
  }
  for (int i=0; i<10; i++) {
    buf[30+i*3+0] = 10;
    buf[30+i*3+1] = 0;
    buf[30+i*3+2] = 0;
  }
  
  flash.writeBytes(patterns[0].address,(byte *)buf,patterns[0].len);
  dumpPage(0x300);
  */
  
  patternCount = loadPatterns();
  Serial.print("loaded patterns: ");
  Serial.println(patternCount);
  selectedPattern = 0;
  
  /*
  
  Serial.println("generating new pattern");
  PatternMetadata newpat;
  char foo[] = "foo";
  memcpy(newpat.name, foo, 4);
  newpat.address = 0x400;
  newpat.len = 10*3*2;
  newpat.frames = 2;
  newpat.flags = 0;
  newpat.fps = 1;
  for (int i=0; i<10; i++) {
    buf[i*3+0] = 10;
    buf[i*3+1] = 10;
    buf[i*3+2] = 10;
  }
  for (int i=0; i<10; i++) {
    buf[30+i*3+0] = 0;
    buf[30+i*3+1] = 10;
    buf[30+i*3+2] = 0;
  }
  
  patternCount = 1;
  int newpatindex = saveLedPattern(&newpat,(byte *)buf);
  Serial.print("new pat index: ");
  Serial.println(newpatindex);
  
  Serial.print("address: ");
  Serial.println((&patterns[selectedPattern])->address);
  Serial.print("len: ");
  Serial.println((&patterns[selectedPattern])->len);
  Serial.print("name: ");
  Serial.println((&patterns[selectedPattern])->name);
  Serial.print("fps: ");
  Serial.println((&patterns[selectedPattern])->fps);

  
  while(1) {
    if (Serial.available()) {
      while(Serial.available()) Serial.read();
      selectedPattern ++;
      if (selectedPattern > 1) selectedPattern = 0;
      for (int i=0; i<sizeof(PatternMetadata); i++) {
        Serial.print((((byte *)(&patterns[selectedPattern]))[i]),HEX);
        Serial.print(" ");
      }
      Serial.println();
      Serial.print("address: ");
      Serial.println((&patterns[selectedPattern])->address);
      Serial.print("len: ");
      Serial.println((&patterns[selectedPattern])->len);
      Serial.print("name: ");
      Serial.println((&patterns[selectedPattern])->name);
      Serial.print("fps: ");
      Serial.println((&patterns[selectedPattern])->fps);
    }
    patternTick();
    writeWS2812(leds,150,ledPin,true); 
    delay(1000);
  }
  
  */
   
  if (debug) Serial.println();
  if (debug) Serial.print("Connecting to ");
  if (debug) Serial.println(myssid);
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(myssid, mypassword);
  while (WiFi.status() != WL_CONNECTED) {
    tick();
  }

  if (debug) Serial.print("Connected with IP:");
  if (debug) Serial.println(WiFi.localIP());
}

int saveLedPattern(struct PatternMetadata * pat, byte * payload) {
  int index = patternCount;
  memcpy(&patterns[index],pat,sizeof(PatternMetadata));
  Serial.print("saving pattern: ");
  Serial.println(sizeof(PatternMetadata));
  for (int i=0; i<sizeof(PatternMetadata); i++) {
    Serial.print((((byte *)(&patterns[index]))[i]),HEX);
    Serial.print(" ");
  }
  Serial.println();
  flash.writeBytes(0x100+sizeof(PatternMetadata)*index,(byte *)(&patterns[index]),sizeof(PatternMetadata));
  
  Serial.println("payload: ");
  for (int i=0; i<pat->len; i++) {
    if (i % 16 == 0 && i != 0) Serial.println();
    Serial.print("0x");
    if (payload[i] < 0x10) Serial.print("0");
    Serial.print(payload[i],HEX);
    Serial.print(" ");
  }
  Serial.println();
  
  flash.writeBytes(pat->address,(byte *)payload,pat->len);

  dumpPage(0x100+sizeof(PatternMetadata)*index);
  dumpPage(pat->address);  
  patternCount ++;
  return index;
}


int loadPatterns() {
  flash.readBytes(0x100,(byte *)patterns,0x200);
  PatternMetadata * ptr = patterns;
  for (int i=0; i<MAX_PATTERNS; i++) {
    Serial.print("address field: ");
    Serial.println(ptr->address);
    if (ptr->address == 0xffff) return i;
    ptr++;
  }
  return 20;
}

char packetBuffer[255];
int readPortFromPacket() {
  int len = udp.read(packetBuffer, 255);
  if (len <= 0) return -1;
  
  char * index = strstr(packetBuffer,"PORT:");
  if (index == NULL) return -1;
  
  index += 5;
  char * index2 = strstr(index,"\r\n");
  if (index2 == NULL) return -1;
  
  index2[0] = 0;
  return atoi(index); 
}

void sendMacAddress() {
  tcp.print("id:");
  for (int i=0; i<WL_MAC_ADDR_LENGTH; i++) {
    if (i != 0) tcp.print(":");
    tcp.print(macAddr[i]);
  }
  tcp.println(); 
}

char serialBuffer[100];
char serialIndex = 0;
void handleSerial() {
  if (Serial.available()) {
    while(Serial.available()) {
      yield();
      char c = Serial.read();
      if (c == '\r') continue;
      if (c == '\n') {
        serialLine();
      }
      serialBuffer[serialIndex++] = c;
      serialBuffer[serialIndex] = 0;
    }
  }
}

void serialLine() {
  serialBuffer[serialIndex] = 0;
  if (strstr(serialBuffer,"ping") != NULL) {
    Serial.println("pong");
  } else if (strstr(serialBuffer,"mac") != NULL) {
    for (int i=0; i<WL_MAC_ADDR_LENGTH; i++) {
      if (i != 0) Serial.print(":");
      Serial.print(macAddr[i]);
    } 
    Serial.println();
  }
}

void loadLedsFromBuffer(char * buf) {
  uint16_t i = 0;
  char * nextToken;
  while(buf != NULL) {
    nextToken = strstr(buf,",");
    if (nextToken != NULL) {
      nextToken[0] = 0;
      nextToken ++;
    }
    
    leds[i++] = atoi(buf);
    
    buf = nextToken;
  } 
}

void processBuffer(char * buf) {
  if (debug) Serial.print("buffer: ");
  if (debug) Serial.println(buf);
  
  if (strstr(buf,"status")) {
    if (debug) Serial.println("status request rec");
    tcp.write("ready\n");
  } else if (strstr(buf,"set:")) {
    if (debug) Serial.println("led data rec");
    loadLedsFromBuffer(buf+4);
    tcp.write("ready\n");    
  }
}

int frame = 0;
long lastFrame = 0;
void patternTick() {
  if (selectedPattern == -1) return;
  PatternMetadata * active = &patterns[selectedPattern];
  if (millis() - lastFrame < 1000.0  / active->fps) return; //wait for a frame based on fps
//  selectedPattern->address
//  selectedPattern->len
//  selectedPattern->flags
//  selectedPattern->frames
//  selectedPattern->fps
  int width = active->len / (active->frames * 3); //width in pixels of the pattern
  Serial.print("width: ");
  Serial.println(width);
  uint16_t startAddress = active->address + (width * 3 * frame);
  Serial.print("startaddr: ");
  Serial.println(startAddress,HEX);
  Serial.print("startaddr: ");
  Serial.println(startAddress,DEC);
  Serial.print("len: ");
  Serial.println(width*3);
  flash.readBytes(startAddress,(byte*)buf,width*3);
  
  for (int i=0; i<stripLength; i++) {
    leds[3*i+0] = buf[(3*(i % width))+0];
    leds[3*i+1] = buf[(3*(i % width))+1];
    leds[3*i+2] = buf[(3*(i % width))+2];
  }
  Serial.println("Wrote leds...");
  frame += 1;
  if (frame >= active->frames) {
    frame = 0;
  }
  Serial.print("next frame: ");
  Serial.println(frame);
}

void tick() {
  yield();
  
  //handleSerial();
  patternTick();
  writeWS2812(leds,150,ledPin,true);
}

void loop() {
  Serial.println("listening for UDP packets");
  udp.begin(2836);
  while(true) {
    tick();
    int packetLength = udp.parsePacket();
    if (packetLength) {
      if (debug) Serial.print("got packet: ");
      if (debug) Serial.println(packetLength);
      IPAddress remoteIp = udp.remoteIP();
      if (debug) Serial.print("remote IP: ");
      if (debug) Serial.println(remoteIp);
      int port = readPortFromPacket();
      if (debug) Serial.print("port: ");
      if (debug) Serial.println(port);
      
      if (!port) break;
      
      udp.stop();
      tcp.connect(remoteIp,port);
      
      sendMacAddress();

      char buf[2000];
      int bindex = 0;
      while(tcp.connected()) {
        while(tcp.available()) {
          char c = tcp.read();
          if (c == '\r') continue;
          if (c == '\n') {
            buf[bindex] = 0;
            processBuffer(buf);
            bindex = 0;
          } else {
            buf[bindex++] = c;
          }
        }
        tick();
      }
      
      Serial.println("Disconnected from server");
      break;
    }
  }
}

void SEND_WS_0(uint8_t ledPin) {
        digitalWrite(ledPin,0);
        digitalWrite(ledPin,1);
        __asm__("nop\n\t");
        __asm__("nop\n\t");
        __asm__("nop\n\t");
        __asm__("nop\n\t");
        __asm__("nop\n\t");
        __asm__("nop\n\t");
        __asm__("nop\n\t");
        __asm__("nop\n\t");
        __asm__("nop\n\t");
        __asm__("nop\n\t");
        __asm__("nop\n\t");
        __asm__("nop\n\t");
        __asm__("nop\n\t");
        __asm__("nop\n\t");
}

void SEND_WS_1(uint8_t ledPin) {
        digitalWrite(ledPin,0);
        __asm__("nop\n\t");
        __asm__("nop\n\t");
        __asm__("nop\n\t");
        __asm__("nop\n\t");
        __asm__("nop\n\t");
        __asm__("nop\n\t");
        __asm__("nop\n\t");
        __asm__("nop\n\t");
        __asm__("nop\n\t");
        __asm__("nop\n\t");
        __asm__("nop\n\t");
        __asm__("nop\n\t");
        __asm__("nop\n\t");
        __asm__("nop\n\t");
        __asm__("nop\n\t");
        __asm__("nop\n\t");
        __asm__("nop\n\t");
        __asm__("nop\n\t");
        __asm__("nop\n\t");
        __asm__("nop\n\t");
        __asm__("nop\n\t");
        __asm__("nop\n\t");
        digitalWrite(ledPin,1);
}


void writeWS2812( uint8_t * buf, uint16_t leds, uint8_t ledPin, bool invert) {
  //TODO see if we can remove this.. it seems like it should be unnecessary.. but I was getting flickering LEDs without it
  digitalWrite(ledPin,1);
  delayMicroseconds(50);
  noInterrupts();
  uint16_t i;
  uint8_t b;
  uint16_t len = leds*3;
  if (invert) {
      for( i = 0; i < len; i++ ) {
          b = buf[i];
          if( b & 0x80 ) SEND_WS_1(ledPin); else SEND_WS_0(ledPin);
          if( b & 0x40 ) SEND_WS_1(ledPin); else SEND_WS_0(ledPin);
          if( b & 0x20 ) SEND_WS_1(ledPin); else SEND_WS_0(ledPin);
          if( b & 0x10 ) SEND_WS_1(ledPin); else SEND_WS_0(ledPin);
          if( b & 0x08 ) SEND_WS_1(ledPin); else SEND_WS_0(ledPin);
          if( b & 0x04 ) SEND_WS_1(ledPin); else SEND_WS_0(ledPin);
          if( b & 0x02 ) SEND_WS_1(ledPin); else SEND_WS_0(ledPin);
          if( b & 0x01 ) SEND_WS_1(ledPin); else SEND_WS_0(ledPin);
      }
  } else {
      for( i = 0; i < len; i++ ) {
          b = buf[i];
          if( b & 0x80 ) SEND_WS_0(ledPin); else SEND_WS_1(ledPin);
          if( b & 0x40 ) SEND_WS_0(ledPin); else SEND_WS_1(ledPin);
          if( b & 0x20 ) SEND_WS_0(ledPin); else SEND_WS_1(ledPin);
          if( b & 0x10 ) SEND_WS_0(ledPin); else SEND_WS_1(ledPin);
          if( b & 0x08 ) SEND_WS_0(ledPin); else SEND_WS_1(ledPin);
          if( b & 0x04 ) SEND_WS_0(ledPin); else SEND_WS_1(ledPin);
          if( b & 0x02 ) SEND_WS_0(ledPin); else SEND_WS_1(ledPin);
          if( b & 0x01 ) SEND_WS_0(ledPin); else SEND_WS_1(ledPin);
      }
  }
  interrupts();
}


  /*
  //ESP AP configuration code
  WiFi.disconnect();
  delay(100);
  WiFi.mode(WIFI_AP);
  WiFi.begin(configssid, "");
  WiFi.disconnect();
  delay(100);
  WiFi.mode(WIFI_AP);
  WiFi.softAP(configssid);
  WiFi.printDiag(Serial);
  delay(1000);
  Serial.println("beginning tcp server");
  tcpserver.begin();
  while(1) {
    WiFiClient client = tcpserver.available();
    if (!client) {
      continue;
    }
    
    // Wait until the client sends some data
    Serial.println("new client");
    int i=0;
    while(!client.available()) yield();
    Serial.println("data incoming..");
    while(client.available()){
      buf[i++] = client.read();
      delay(0);
    }
    buf[i] = 0;
    Serial.println("buffer:");
    Serial.println(buf);
    
    char * a = strstr(buf,"\n");
    a[0] = 0;
    
    a = strstr(buf,"ssid=");
    if (a != NULL) {
      a += 5;
      char * b = strstr(a,"&");
      if (b == NULL) break;
      b[0] = 0;
      strcpy(myssid,a);
      Serial.print("ssid: ");
      Serial.println(myssid);

      a = strstr(b+1,"password=");
      if (a == NULL) break;
      a += 9;
      b = strstr(a," ");
      if (b == NULL) break;
      b[0] = 0;
      strcpy(mypassword,a);
      Serial.print("pasword: ");
      Serial.println(mypassword);
      
      break;
    }
    
    String s = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n<!DOCTYPE HTML>\r\n<html>\r\n";
    s += "<form method='GET' action='/'>";
    s += "<input name='ssid'>";
    s += "<input name='password'>";
    s += "<input type='submit'>";
    s += "</form>";
    s += "</html>\n";
  
    // Send the response to the client
    client.print(s);
    delay(1);
    Serial.println("Client disonnected");

    yield();
  }
  */
  
    /*
  delay(5000);
  Serial.println("testing flash memroy");
  //test flash
  int dblen = 300;
  for (int i=0; i<dblen; i++) {
    buf[i] = i;
  }
  flash.writeBytes(0,(byte*)buf,dblen);

  dumpPage(0x0);
  dumpPage(0x100);
  
  flash.readBytes(200,(byte*)buf,100);
  for (int i=0; i<100; i++) {
    if (i % 16 == 0 && i != 0) Serial.println();
    Serial.print("0x");
    if (buf[i] < 0x10) Serial.print("0");
    Serial.print(buf[i],HEX);
    Serial.print(" ");
  }
  Serial.println();

  while(true) {    
    delay(1000);
  }
  //test flash
  */


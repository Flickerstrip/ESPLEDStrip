#include <SoftwareSPI.h>
#include <ESP8266WiFi.h>
#include <FlashMemory.h>
#include <ESPWS2812.h>
#include <EEPROM.h>

#include "NetworkManager.h"

#include "Arduino.h"

#define SPI_SCK 5
#define SPI_MOSI 14
#define SPI_MISO 16
#define MEM_CS 4
 
int ledPin = 13;
FlashMemory flash(SPI_SCK,SPI_MOSI,SPI_MISO,MEM_CS);
ESPWS2812 strip(ledPin,150,true);
NetworkManager network;
uint8_t macAddr[WL_MAC_ADDR_LENGTH];
uint16_t stripLength = 150;
uint8_t leds[450];

bool debug = true;
const char* configssid = "esp8266confignetwork";

char buf[1000];
struct PatternMetadata {
  char name[16];
  uint32_t address;
  uint32_t len;
  uint16_t frames;
  uint8_t flags;
  uint8_t fps;
};

struct Configuration {
    char ssid[50];
    char password[50];
    byte selectedPattern;
};

struct PacketStructure {
	uint32_t type;
	uint32_t param1;
	uint32_t param2;
};

const int MAX_PATTERNS = 17;
int patternCount = 0;
PatternMetadata patterns[MAX_PATTERNS];
int lastSavedPattern = -1;

Configuration config;

void setup() {
  Serial.begin(115200);
  delay(10);
 
  strip.begin();

  WiFi.macAddress(macAddr);
  
  Serial.println("\n");

  delay(1000); //TODO figure out why the memory chip needs some time to start up and add it to the library

  //factoryReset();

  loadConfiguration();

  patternCount = loadPatterns();
  if (patternCount != 0) selectPattern(0);

  if (debug) Serial.print("Connecting to ");
  if (debug) Serial.println(config.ssid);
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(config.ssid, config.password);
  while (WiFi.status() != WL_CONNECTED) {
    tick();
  }

  if (debug) Serial.print("Connected with IP:");
  if (debug) Serial.println(WiFi.localIP());
}

void factoryReset() {
  Serial.println("Resetting to Factory Defaults");
  clearPatterns();

  generateDefaultPatterns();

  EEPROM.begin(sizeof(Configuration));
  for (int i=0; i<sizeof(Configuration); i++) {
    EEPROM.write(i,0xff);

  }
  EEPROM.end();

  loadConfiguration();
}

void loadConfiguration() {
  EEPROM.begin(sizeof(Configuration));
  for (int i=0; i<sizeof(Configuration); i++) {
    ((byte *)(&config))[i] = EEPROM.read(i);
  }
  EEPROM.end();

  //make these valid strings of length 0
  if (config.ssid[0] == 255) config.ssid[0] = 0;
  if (config.password[0] == 255) config.password[0] = 0;
}

void saveConfiguration() {
  EEPROM.begin(sizeof(Configuration));
  for (int i=0; i<sizeof(Configuration); i++) {
    EEPROM.write(i,((byte *)(&config))[i]);

  }
  EEPROM.end();
}

void setNetwork(String ssid,String password) {
  ssid.toCharArray((char*)&config.ssid,50);
  password.toCharArray((char *)&config.password,50);
}

void generateDefaultPatterns() {
  Serial.println("generating new pattern");
  PatternMetadata newpat;
  char foo[] = "TwoBlink";
  memcpy(newpat.name, foo, strlen(foo)+1);
  newpat.len = 10*3*2;
  newpat.frames = 2;
  newpat.flags = 0;
  newpat.fps = 1;
  for (int i=0; i<10; i++) {
    buf[i*3+0] = 255;
    buf[i*3+1] = 255;
    buf[i*3+2] = 255;
  }
  for (int i=0; i<10; i++) {
    buf[30+i*3+0] = 255;
    buf[30+i*3+1] = 0;
    buf[30+i*3+2] = 0;
  }

  byte patindex = saveLedPatternMetadata(&newpat);
  saveLedPatternBody(patindex,0,(byte*)buf,10*3*2);
}

void clearPatterns() {
  flash.erasePage(0x100);
  flash.erasePage(0x200);
  patternCount = 0;
  config.selectedPattern = 0xff;
  lastSavedPattern = -1;
}

uint32_t findInsertLocation(uint32_t len) {
  for (int i=0; i<patternCount; i++) {
    uint32_t firstAvailablePage = ((patterns[i].address + patterns[i].len) & 0xffffff00) + 0x100;

    if (i == patternCount - 1) return i+1; //last pattern

    uint32_t spaceAfter = patterns[i+1].address - firstAvailablePage;
    if (spaceAfter > len) {
      return i+1;
    }
  }
  return 0;
}

byte saveLedPatternMetadata(struct PatternMetadata * pat) {
  byte insert = findInsertLocation(pat->len);
  if (insert == 0) {
      pat->address = 0x300;
  } else {
      //address is on the page after the previous pattern
      pat->address = ((patterns[insert-1].address + patterns[insert-1].len) & 0xffffff00) + 0x100;
  }

  for (int i=patternCount; i>insert; i--) {
    memcpy(&patterns[i],&patterns[i-1],sizeof(PatternMetadata));
  }
  patternCount ++;

  memcpy(&patterns[insert],pat,sizeof(PatternMetadata));
  flash.writeBytes(0x100+sizeof(PatternMetadata)*insert,(byte *)(&patterns[insert]),(patternCount-insert)*sizeof(PatternMetadata));

  return insert;
}

void saveLedPatternBody(int pattern, uint32_t patternStartPage, byte * payload, uint32_t len) {
  PatternMetadata * pat = &patterns[pattern];

  uint32_t writeLocation = pat->address + patternStartPage*0x100;
  flash.writeBytes(writeLocation,payload,len);
}

int loadPatterns() {
  flash.readBytes(0x100,(byte *)patterns,0x200);
  PatternMetadata * ptr = patterns;
  for (int i=0; i<MAX_PATTERNS; i++) {
    if (ptr->address == 0xffffffff) return i;
    ptr++;
  }
  return 20;
}

void sendMacAddress() {
  network.getTcp()->print("id:");
  for (int i=0; i<WL_MAC_ADDR_LENGTH; i++) {
    if (i != 0) network.getTcp()->print(":");
    network.getTcp()->print(macAddr[i]);
  }
  network.getTcp()->println();
  network.getTcp()->println(); 
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

void deletePattern(byte n) {
  patternCount--;
  for (int i=n; i<patternCount; i++) {
    memcpy(&patterns[i],&patterns[i+1],sizeof(PatternMetadata));
  }
  if (n <= config.selectedPattern) {
    selectPattern(config.selectedPattern-1);
  }

  byte * ptr = (byte*)(&patterns[patternCount]);
  for (int i=0; i<sizeof(PatternMetadata); i++) {
    ptr[i] = 0xff;
  }
  flash.writeBytes(0x100+sizeof(PatternMetadata)*n,(byte *)(&patterns[n]),(patternCount-n+1)*sizeof(PatternMetadata));
}

const byte UNUSED = 0;
const byte PING = 1;
const byte GET_PATTERNS = 2;
const byte CLEAR_PATTERNS = 3;
const byte DELETE_PATTERN = 4;
const byte SELECT_PATTERN = 5;
const byte SAVE_PATTERN = 6;
const byte PATTERN_BODY = 7;

void processBuffer(byte * buf, int len) {
  PacketStructure * packet = (PacketStructure*)buf;
  buf = buf + sizeof(PacketStructure);
  len = len - sizeof(PacketStructure);

  if (packet->type == PING) {
    //just respond with ready (below)
  } else if (packet->type == DELETE_PATTERN) {
    deletePattern(packet->param1);
  } else if (packet->type == CLEAR_PATTERNS) {
    clearPatterns();
  } else if (packet->type == SELECT_PATTERN) {
    byte pattern = packet->param1;
    selectPattern(pattern);
  } else if (packet->type == GET_PATTERNS) {
    PatternMetadata * pat;
    int index = 0;
    char * wh = (char *)buf;
    for (int i=0; i<patternCount; i++) {
        if (i != 0) network.getTcp()->write("\n");
        pat = &patterns[i];
        int n = sprintf(wh,"%d,%s,%d,%d,%d,%d,%d\n",i,pat->name,pat->address,pat->len,pat->frames,pat->flags,pat->fps);
        index += n;
        wh += n;
    }
    network.getTcp()->write("patterns\n");
    network.getTcp()->write((byte*)buf,index);
    network.getTcp()->write("\n"); //already have one at end of line
  } else if (packet->type == SAVE_PATTERN) {
    byte * start = buf;
    PatternMetadata pat;
    //BE AWARE: word alignment seems to matter.. we're copying this to a different location to avoid pointer alignment issues
    memcpy(&pat,buf,sizeof(PatternMetadata));

    start = start + sizeof(PatternMetadata); //start of payload
    byte pattern = saveLedPatternMetadata(&pat);

    lastSavedPattern = pattern;
    uint32_t remaining = len - (start-(byte*)buf);
    if (remaining > 0) {
      saveLedPatternBody(pattern,0,(byte*)start,remaining);
    }
    if (pattern == -1) {
        Serial.println("writing failed ready out");
        network.getTcp()->write("FAILED\n\n");
    } else {
        selectPattern(pattern);
    }
  } else if (packet->type == PATTERN_BODY) {
    byte pattern = packet->param1;
    uint32_t patternPage = packet->param2;

    if (pattern == 0xff) pattern = lastSavedPattern;

    saveLedPatternBody(pattern,patternPage,buf,len);
  }
  network.getTcp()->write("ready\n\n");
}

int frame = 0;
long lastFrame = 0;
void patternTick() {
  if (config.selectedPattern == 0xff) { //no pattern selected, turn LEDs off
      for (int i=0; i<stripLength; i++) {
        leds[3*i+1] = 0;
        leds[3*i+0] = 0;
        leds[3*i+2] = 0;
      }
      return;
  }

  PatternMetadata * active = &patterns[config.selectedPattern];
  if (millis() - lastFrame < 1000  / active->fps) return; //wait for a frame based on fps
  lastFrame = millis();
  uint32_t width = active->len / (active->frames * 3); //width in pixels of the pattern
  uint32_t startAddress = active->address + (width * 3 * frame);
  
  flash.readBytes(startAddress,(byte*)buf,width*3);
  
  for (int i=0; i<stripLength; i++) {
    //leds[3*i+1] = buf[(3*(i % width))+0]; //we reverse the byte order, strip reads in GRB, stored in RGB
    //leds[3*i+0] = buf[(3*(i % width))+1];
    //leds[3*i+2] = buf[(3*(i % width))+2];

    leds[3*i+1] = buf[(3*(i % width))+0] >> 4; //we reverse the byte order, strip reads in GRB, stored in RGB
    leds[3*i+0] = buf[(3*(i % width))+1] >> 4;
    leds[3*i+2] = buf[(3*(i % width))+2] >> 4;
  }
  frame += 1;
  if (frame >= active->frames) {
    frame = 0;
  }
}

void nextMode() {
    if (config.selectedPattern == 0xff) {
        selectPattern(0);
    } else if(config.selectedPattern+1 >= patternCount) {
        selectPattern(0xff);
    } else {
        selectPattern(config.selectedPattern+1);
    }
}

void selectPattern(byte n) {
    config.selectedPattern = n;
    if (config.selectedPattern == 0xff) {
        Serial.println("Mode set to 0xff, LEDs off");
    } else {
        PatternMetadata * pat = &patterns[n];
        Serial.print("Selected pattern: ");
        Serial.println(pat->name);
    }
    saveConfiguration();
    
    frame = 0;
    lastFrame = 0;
}

void tick() {
  yield();
  
  //handleSerial();
  patternTick();
  strip.sendLeds(leds);
}


void handleUdpPacket(IPAddress ip, byte * buf, int len) {
  char * charbuf = (char*)buf;
  if (strcmp(charbuf,"announce") == 0) {
    if (!network.isTcpActive()) {
      int port = atoi(charbuf + strlen(charbuf)+1);
      network.startTcp(&ip,port);
      sendMacAddress();
    }
  } else if (strcmp(charbuf,"mode") == 0) {
    nextMode();
  }
}

byte networkBuffer[2000];
void loop() {
  network.startUdp();

  while(true) {
    tick();
    network.tick();

    if (network.isUdpActive()) {
      if (network.udpPacketAvailable()) {
        IPAddress ip;
        int readLength = network.getUdpPacket(&ip,(byte*)(&networkBuffer),2000);
        handleUdpPacket(ip,(byte*)&networkBuffer,readLength);
      }
    }

    if (network.isTcpActive()) {
      if (network.tcpPacketAvailable()) {
        int readLength = network.getTcpPacket((byte*)&networkBuffer,2000);
        processBuffer((byte*)&networkBuffer,readLength);
      }
    }
  }
}

/////////////////////////////////////////////////////////////

void debugHex(char *buf, int len) {
    for (int i=0; i<len; i++) {
        Serial.print(" ");
        if (buf[i] >= 32 && buf[i] <= 126) {
            Serial.print(buf[i]);
        } else {
            Serial.print(" ");
        }
        Serial.print(" ");
    }
    Serial.println();

    for (int i=0; i<len; i++) {
        if (buf[i] < 16) Serial.print("0");
        Serial.print(buf[i],HEX);
        Serial.print(" ");
    }
    Serial.println();
}


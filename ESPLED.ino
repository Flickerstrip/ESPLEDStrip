// vim:ts=2 sw=2:
#include <SoftwareSPI.h>
#include <ESP8266WiFi.h>
#include <FlashMemory.h>
#include <ESPWS2812.h>
#include <EEPROM.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <DNSServer.h>

#include "NetworkManager.h"
#include "PatternManager.h"
#include "CaptivePortalConfigurator.h"

#include "Arduino.h"

#define SPI_SCK 5
#define SPI_MOSI 14
#define SPI_MISO 16
#define MEM_CS 4
#define LED_STRIP 13

FlashMemory flash(SPI_SCK,SPI_MOSI,SPI_MISO,MEM_CS);
ESPWS2812 strip(LED_STRIP,150,true);
NetworkManager network;
PatternManager patternManager(&flash);
CaptivePortalConfigurator cpc("esp8266confignetwork");

uint8_t macAddr[WL_MAC_ADDR_LENGTH];
uint16_t stripLength = 150;
uint8_t leds[450];
int lastSavedPattern = -1;
bool disconnect = false;

bool debug = true;
struct Configuration {
  char ssid[50];
  char password[50];
  byte selectedPattern;
  byte brightness;
};

struct PacketStructure {
	uint32_t type;
	uint32_t param1;
	uint32_t param2;
};

Configuration config;

/////////////
int indicatorFrame = 0;
int indicatorLength = 0;
byte indicatorColor[] = {50,0,0};
unsigned long lastFrameTime;
bool doIndicator = false;
/////////////


void setup() {
  Serial.begin(115200);
  delay(10);

  strip.begin();

  WiFi.macAddress(macAddr);
  
  Serial.println("\n\n");

  /*
  while(1) {
    delay(4000);
    for (int i=0; i<20; i++) {
      int a = analogRead(A0);
      Serial.println(a);
      delay(1);
    }
    Serial.println("Done\n");
  }
  */

  startupPattern();

  //factoryReset();

  loadConfiguration();
  patternManager.loadPatterns();
  Serial.print("Loaded patterns: ");
  Serial.println(patternManager.getPatternCount());
  patternManager.selectPattern(config.selectedPattern);
}

void startupPattern() {
  fillStrip(10,10,25);
  strip.sendLeds(leds);
  delay(300);
  fillStrip(25,10,10);
  strip.sendLeds(leds);
  delay(300);
}

void fillStrip(byte r, byte g, byte b) {
  for (int i=0; i<stripLength; i++) {
    leds[i*3] = g;
    leds[i*3+1] = r;
    leds[i*3+2] = b;
  }
}

void factoryReset() {
  Serial.println("Resetting to Factory Defaults");
  patternManager.resetPatternsToDefault();

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
  //if (config.selectedPattern == 255) config.selectedPattern = 0;
  //if (config.brightness == 255) config.brightness = 10;
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


const byte UNUSED = 0;
const byte PING = 1;
const byte GET_STATUS = 2;
const byte CLEAR_PATTERNS = 3;
const byte DELETE_PATTERN = 4;
const byte SELECT_PATTERN = 5;
const byte SAVE_PATTERN = 6;
const byte PATTERN_BODY = 7;
const byte DISCONNECT_NETWORK = 8;
const byte SET_BRIGHTNESS = 9;

unsigned long lastStart;
void start() {
  lastStart = millis();
}

void stop(String s) {
  Serial.print(s);
  Serial.print(": ");
  Serial.print(millis()-lastStart);
  Serial.print("ms");
  Serial.println();
}

void selectPattern(byte pattern) {
    patternManager.selectPattern(pattern);
    config.selectedPattern = patternManager.getSelectedPattern();
    saveConfiguration();
}

void processBuffer(byte * buf, int len) {
  //if (buf[0] != PING) debugHex((char*)buf,len > 20 ? 20 : len);

  PacketStructure * packet = (PacketStructure*)buf;
  buf = buf + sizeof(PacketStructure);
  len = len - sizeof(PacketStructure);

  if (packet->type == PING) {
    //just respond with ready (below)
  } else if (packet->type == DELETE_PATTERN) {
    patternManager.deletePattern(packet->param1);
  } else if (packet->type == CLEAR_PATTERNS) {
    patternManager.clearPatterns();
  } else if (packet->type == SELECT_PATTERN) {
    byte pattern = packet->param1;
    selectPattern(pattern);
  } else if (packet->type == GET_STATUS) {
    sendStatus();
  } else if (packet->type == SAVE_PATTERN) {
    byte * start = buf;
    //BE AWARE: word alignment seems to matter.. we're copying this to a different location to avoid pointer alignment issues
    PatternManager::PatternMetadata pat;
    memcpy(&pat,buf,sizeof(PatternManager::PatternMetadata));

    byte pattern = patternManager.saveLedPatternMetadata(&pat);

    lastSavedPattern = pattern;
    start = start + sizeof(PatternManager::PatternMetadata); //start of payload
    uint32_t remaining = len - (start-(byte*)buf);
    if (remaining > 0) {
      patternManager.saveLedPatternBody(pattern,0,(byte*)start,remaining);
    }

    selectPattern(pattern);
  } else if (packet->type == PATTERN_BODY) {
    byte pattern = packet->param1;
    uint32_t patternPage = packet->param2;

    if (pattern == 0xff) pattern = lastSavedPattern;

    patternManager.saveLedPatternBody(pattern,patternPage,buf,len);
  } else if (packet->type == DISCONNECT_NETWORK) {
    disconnect = true;
  } else if (packet->type == SET_BRIGHTNESS) {
    config.brightness = packet->param1;
    Serial.print("Set brightness to: ");
    Serial.println(config.brightness);
    saveConfiguration();
    sendStatus();
  }
  network.getTcp()->write("ready\n\n");
}

/*
pat->name,pat->address,pat->len,pat->frames,pat->flags,pat->fps
{
  'patterns':[
    {
      'name':'Strip name',
      'address':addy,
      'length':len,
      'frame':frames,
      'flags':flags,
      'fps':fps
    }
  ],
  'selectedPattern':5,
  'brightness':50,
  'memory':{
    'used':100,
    'free':100,
    'total':100
  }
}

*/

void sendStatus() {
  int bufferSize = 1000;
  byte jsonBuffer[bufferSize];
  char * ptr = (char*)jsonBuffer;
  int size;

  size = snprintf(ptr,bufferSize,"{\"selectedPattern\":%d,\"brightness\":%d,\"memory\":{\"used\":%d,\"free\":%d,\"total\":%d},\"patterns\":",patternManager.getSelectedPattern(),config.brightness,patternManager.getUsedBlocks(),patternManager.getAvailableBlocks(),patternManager.getTotalBlocks());
  bufferSize -= size;
  ptr += size;

  size = patternManager.serializePatterns(ptr,bufferSize);
  bufferSize -= size;
  ptr += size;

  size = snprintf(ptr,bufferSize,"}");
  bufferSize -= size;
  ptr += size;

  network.getTcp()->write((uint8_t *)jsonBuffer,(size_t)((int)ptr-(int)jsonBuffer));
  network.getTcp()->write("\n\n"); //already have one at end of line
}

void patternTick() {
  byte brightness = (255*config.brightness)/100;
  bool hasNewFrame = patternManager.loadNextFrame(leds,stripLength,brightness);
  if (hasNewFrame) strip.sendLeds(leds);
}

void nextMode() {
  if (patternManager.getSelectedPattern() == -1) {
    selectPattern(0);
  } else if(config.selectedPattern+1 >= patternManager.getPatternCount()) {
    selectPattern(-1);
  } else {
    selectPattern(patternManager.getSelectedPattern()+1);
  }
}

void indicateGreenFast() {
  doIndicator = true;
  indicatorColor[0] = 0;
  indicatorColor[1] = 50;
  indicatorColor[2] = 0;
  lastFrameTime = 0;
  indicatorFrame = 0;
  indicatorLength = 20;
}

void indicateBlueSlow() {
  doIndicator = true;
  indicatorColor[0] = 0;
  indicatorColor[1] = 0;
  indicatorColor[2] = 50;
  lastFrameTime = 0;
  indicatorFrame = 0;
  indicatorLength = 60;
}

void indicateStop() {
  doIndicator = false;
}

void indicatorTick() {
  if (millis() - lastFrameTime < 1000/30) return;
  lastFrameTime = millis();

  int halfLength = indicatorLength >> 1;
  int phase = (indicatorFrame > halfLength) ? (indicatorLength - indicatorFrame) : indicatorFrame;
  float brightness = (float)phase/(float)halfLength;

  byte r = brightness*(indicatorColor[0]>>1) + (indicatorColor[0]>>1);
  byte g = brightness*(indicatorColor[1]>>1) + (indicatorColor[1]>>1);
  byte b = brightness*(indicatorColor[2]>>1) + (indicatorColor[2]>>1);
  fillStrip(r,g,b);
  strip.sendLeds(leds);

  indicatorFrame ++;
  if (indicatorFrame > indicatorLength) indicatorFrame = 0;
}

void tick() {
  yield();
  
  //handleSerial();
  if (doIndicator) {
    indicatorTick();
  } else {
    patternTick();
  }
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
  bool timedout = false;
  while(true) {
    if (timedout || strlen(config.ssid) == 0) {
      //Handle configuration AP using a Captive Portal
      cpc.begin();
      indicateBlueSlow();
      while(!cpc.hasConfiguration()) {
        cpc.tick();
        tick();
        delay(1);
      }
      indicateStop();

      cpc.getSSID().toCharArray(config.ssid,50);
      cpc.getPassword().toCharArray(config.password,50);

      saveConfiguration();
      timedout = false;

      Serial.print("Configured via captive portal: ");
      Serial.println(cpc.getSSID());
    } else {
      if (debug) Serial.print("Connecting to ");
      if (debug) Serial.println(config.ssid);

      WiFi.mode(WIFI_STA);
      WiFi.begin(config.ssid, config.password);

      unsigned long start = millis();
      unsigned long timeoutDuration = 15000;

      indicateGreenFast();
      while (WiFi.status() != WL_CONNECTED) {
        if (millis() - start >= timeoutDuration) break;
        delay(1);
        tick();
      }
      indicateStop();

      if (WiFi.status() != WL_CONNECTED) {
        Serial.println("Timed out!");
        timedout = true;
        continue;
      }

      timedout = false;

      if (debug) Serial.print("Connected with IP:");
      if (debug) Serial.println(WiFi.localIP());

      disconnect = false;
      while(WiFi.status() == WL_CONNECTED) {
        handleConnectedState();
        delay(1);

        if (disconnect) {
          WiFi.disconnect();
          config.ssid[0] = 0;
          config.password[0] = 0;
          saveConfiguration();
          break;
        }
      }
      network.stop();
      Serial.println("Disconnected from AP");
    }
  }
}

void handleConnectedState() {
  if (!network.isUdpActive()) {
    network.startUdp();
  }

  tick();

  bool tcpActive = network.isTcpActive();
  network.tick();
  if (tcpActive && !network.isTcpActive()) {
    Serial.println("TCP Disconnected");
  }

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

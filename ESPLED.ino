// vim:ts=2 sw=2:
#include <SoftwareSPI.h>
#include <ESP8266WiFi.h>
#include <FlashMemory.h>
#include <EEPROM.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <DNSServer.h>
#include <ESP8266SSDP.h>
#include <WiFiServer.h>

#include <Adafruit_NeoPixel.h>
#include "NetworkManager.h"
#include "PatternManager.h"
#include "CaptivePortalConfigurator.h"

#include "version.h"

#include "Arduino.h"

#define SPI_SCK 5
#define SPI_MOSI 14
#define SPI_MISO 16
#define MEM_CS 4
#define LED_STRIP 13

#define DEBUG_UPDATER true

void *memchr(const void *s, int c, size_t n)
{
    unsigned char *p = (unsigned char*)s;
    while( n-- )
        if( *p != (unsigned char)c )
            p++;
        else
            return p;
    return 0;
}

uint16_t stripLength = 150;
FlashMemory flash(SPI_SCK,SPI_MOSI,SPI_MISO,MEM_CS);
//ESPWS2812 strip(LED_STRIP,150,true);
Adafruit_NeoPixel strip(stripLength, LED_STRIP, NEO_GRB + NEO_KHZ800);
//NetworkManager network;
PatternManager patternManager(&flash);
//CaptivePortalConfigurator cpc("esp8266confignetwork");
//ESP8266WebServer webserver(80);
WiFiServer server(80);

uint8_t macAddr[WL_MAC_ADDR_LENGTH];
bool disconnect = false;

bool debug = true;
struct Configuration {
  char ssid[50];
  char password[50];
  byte selectedPattern;
  byte brightness;
  byte flags;
};

const byte FLAG_POWER = 7;

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
  strip.show();

  WiFi.macAddress(macAddr);
  
  Serial.println("\n\n");

  Serial.print("Flickerstrip Firmware Version: ");
  Serial.println(GIT_CURRENT_VERSION);

  /*
  while(1) {
    delay(500);
    //for (int i=0; i<20; i++) {
      int a = analogRead(A0);
      Serial.println(a);
      //delay(1);
    //}
    //Serial.println("Done\n");
  }
  */


  loadConfiguration();

  patternManager.loadPatterns();
  Serial.print("Loaded patterns: ");
  Serial.println(patternManager.getPatternCount());
  patternManager.selectPattern(config.selectedPattern);

  /*
  while(1) {
    for (int i=0; i<15; i++) {
      fillStrip(0,i*10,0);
      strip.sendLeds(leds);
      delay(250);
    }
    for (int i=0; i<15; i++) {
      fillStrip(0,(15-i)*10,0);
      strip.sendLeds(leds);
      delay(250);
    }
  }
  */

  //factoryReset();
}

void startupPattern() {
  fillStrip(10,10,25);
  strip.show();
  delay(300);
  fillStrip(25,10,10);
  strip.show();
  delay(300);
}

void fillStrip(byte r, byte g, byte b) {
  for (int i=0; i<stripLength; i++) {
    strip.setPixelColor(i,r,g,b);
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
  if (config.selectedPattern == 255) config.selectedPattern = 0;
  if (config.brightness == 255) config.brightness = 10;
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

int macAddrToString(char * str,int bufferSize) {
  return snprintf(str,bufferSize,"%x:%x:%x:%x:%x:%x",macAddr[0],macAddr[1],macAddr[2],macAddr[3],macAddr[4],macAddr[5]);
}

char serialBuffer[100];
char serialIndex = 0;
void handleSerial() {
  if (Serial.available()) {
    while(Serial.available()) {
      yield();
      char c = Serial.read();
      Serial.write(c);
      if (c == '\r') continue;
      if (c == '\n') {
        serialLine();
        serialIndex = 0;
      }
      serialBuffer[serialIndex++] = c;
      serialBuffer[serialIndex] = 0;
    }
  }
}

void serialLine() {
  debugHex(serialBuffer,serialIndex);
  if (strstr(serialBuffer,"ping") != NULL) {
    Serial.println("pong");
  } else if (strstr(serialBuffer,"mac") != NULL) {
    for (int i=0; i<WL_MAC_ADDR_LENGTH; i++) {
      if (i != 0) Serial.print(":");
      Serial.print(macAddr[i]);
    } 
    Serial.println();
  } else if (strstr(serialBuffer,"dc") != NULL) {
    Serial.println("Resetting wireless configuration");
    config.ssid[0] = 0;
    config.password[0] = 0;
    saveConfiguration();
    ESP.restart();
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
const byte TOGGLE_POWER = 10;
const byte SAVE_TEST_PATTERN = 11;
const byte UPLOAD_FIRMWARE = 12;

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
  patternManager.showTestPattern(false);
  patternManager.selectPattern(pattern);
  config.selectedPattern = patternManager.getSelectedPattern();
  saveConfiguration();
}

void sendHttp(WiFiClient * client, int statusCode, const char * statusText, const char * contentType, const char * content) {
  int contentLength = strlen(content);
  char buffer[contentLength+300];
  int n = snprintf(buffer,contentLength+300,"HTTP/1.0 %d %s\r\nContent-Type: %s\r\nContent-Length:%d\r\n\r\n%s",statusCode,statusText,contentType,strlen(content),content);

  client->write((uint8_t*)buffer,n);
}

void sendOk(WiFiClient * client) {
  char content[] = "{\"type\":\"OK\"}";
  sendHttp(client,200,"OK","application/json",content);
}

void sendErr(WiFiClient * client, const char * err) {
  char content[strlen(err)+50];
  int n = snprintf(content,strlen(err)+50,"{\"type\":\"error\",\"message\":\"%s\"}",err);
  content[n] = 0;
  sendHttp(client,500,"Bad Request","application/json",content);
}

void sendStatus(WiFiClient * client) {
  int bufferSize = 1000;
  char jsonBuffer[bufferSize];
  char * ptr = (char*)jsonBuffer;
  int size;

  size = snprintf(ptr,bufferSize,"{\"type\":\"status\",\"firmware\":\"%s\",\"power\":%d,\"selectedPattern\":%d,\"brightness\":%d,\"memory\":{\"used\":%d,\"free\":%d,\"total\":%d},\"patterns\":",GIT_CURRENT_VERSION,isPowerOn(),patternManager.getSelectedPattern(),config.brightness,patternManager.getUsedBlocks(),patternManager.getAvailableBlocks(),patternManager.getTotalBlocks());
  bufferSize -= size;
  ptr += size;

  size = patternManager.serializePatterns(ptr,bufferSize);
  bufferSize -= size;
  ptr += size;

  size = snprintf(ptr,bufferSize,",\"mac\":\"");
  bufferSize -= size;
  ptr += size;

  size = macAddrToString(ptr,bufferSize);
  bufferSize -= size;
  ptr += size;

  size = snprintf(ptr,bufferSize,"\"");
  bufferSize -= size;
  ptr += size;

  size = snprintf(ptr,bufferSize,"}");
  bufferSize -= size;
  ptr += size;

  ptr[0] = 0;
  sendHttp(client,200,"OK","application/json",jsonBuffer);
}

void patternTick() {
  byte brightness = (255*config.brightness)/100;
  strip.setBrightness(brightness);
  bool hasNewFrame = patternManager.loadNextFrame(strip);
  if (hasNewFrame) strip.show();
}

void nextMode() {
  patternManager.showTestPattern(false);
  if (!isPowerOn()) {
    toggleStrip(true);
  } else if(config.selectedPattern+1 >= patternManager.getPatternCount()) {
    selectPattern(0);
    toggleStrip(false);
  } else {
    selectPattern(patternManager.getSelectedPattern()+1);
  }
}

bool isPowerOn() {
  return (config.flags >> FLAG_POWER) & 1 == 1;
}

void toggleStrip(bool on) {
  if (on) {
    config.flags |= (1 << FLAG_POWER);
  } else {
    config.flags &= ~(1 << FLAG_POWER);
  }
  saveConfiguration();
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
  strip.show();

  indicatorFrame ++;
  if (indicatorFrame > indicatorLength) indicatorFrame = 0;
}

int powerWasOn = true;
void tick() {
  yield();
  
  //handleSerial();
  if (isPowerOn()) {
    powerWasOn = true;
    patternTick();
  } else if (powerWasOn == true) {
    fillStrip(0,0,0);
    strip.show();
    powerWasOn = false;
  }
}


void handleUdpPacket(IPAddress ip, byte * buf, int len) {
  char * charbuf = (char*)buf;
  if (strcmp(charbuf,"mode") == 0) {
    nextMode();
  }
}

int findUrl(const char * buf, char ** loc) {
  char * ptr;

  ptr = strchr(buf,' ');
  ptr++;
  loc[0] = ptr;

  ptr = strchr(ptr,' ');

  return ptr - loc[0];
}

void printFound(int n, char * loc) {
  char tmp[n+1];
  memcpy(&tmp,loc,n);
  tmp[n] = 0;
  Serial.println(tmp);
}

int findPath(const char * buf, char ** loc) {
  char * url;
  int n = findUrl(buf,&url);

  loc[0] = url;

  char * ptr = (char*)memchr(url,'?',n);
  if (ptr == NULL) return n;

  return ptr-url;
}

int findGet(const char * buf, char ** loc, const char * key) {
  char * url;
  int n = findUrl(buf,&url);

  char * ptr = strchr(url,'?');
  if (ptr == NULL) return -1;

  ptr = strstr(ptr+1,key);
  if (ptr == NULL) return -1;

  char * a = strchr(ptr+1,' ');
  char * b = strchr(ptr+1,'&');
  if (b != NULL and (a == NULL or a > b)) {
    a = b;
  }

  if (a == NULL) return -1;

  loc[0] = ptr + strlen(key) + 1;
  return a - loc[0];
}

bool getInteger(const char * buf, const char * key, int * val) {
  char * vloc;
  int vlen = findGet(buf,&vloc,key);
  if (vlen != -1) {
    char cval[vlen+1];
    memcpy(&cval,vloc,vlen);
    cval[vlen] = 0;
    int ival = atoi(cval);
    val[0] = ival;
    return true;
  } else {
    return false;
  }
}

void loadFirmware(WiFiClient & client, uint32_t uploadSize) {
    Serial.print("FIRMWARE: ");
    Serial.println(uploadSize);

    uint32_t totalBytesRead = 0;
    if(!Update.begin(uploadSize)){
      Serial.println("Update Begin Error");
      return;
    } else {
      Serial.println("begin success");
    }

    uint32_t written = 0;
    byte updateBuffer[1000];
    while(!Update.isFinished()) {
      yield();
      int n = readBytes(client,(char*)updateBuffer,1000,2000);
      if (n == 0) {
        Serial.println("readBytes returned nothing!");
        return;
      }
      //Serial.println("running update.write");
      yield();
      written += Update.write(updateBuffer,n);
      yield();
      if (written % 10000 <= 1000) {
        Serial.print(written);
        Serial.print(" of ");
        Serial.print(uploadSize);
        Serial.print(" ");
        Serial.print((written*100)/uploadSize);
        Serial.println("%");
      }
      //if (written > 0) client.write(1);
    }
    Serial.println("end of update loop");

    if(Update.end()){
      Serial.printf("Update Success\nRebooting...\n");
      ESP.restart();
    } else {
      Update.printError(client);
      Update.printError(Serial);
    }
}

int readBytes(WiFiClient & client, char * buf, int length, int timeout) {
  long start = millis();
  int bytesRead = 0;
  while(bytesRead < length) {
    if (client.connected() == false || millis() - start > timeout) break;
    if (client.available()) {
      int readThisTime = client.read((uint8_t*)buf,length-bytesRead);
      bytesRead += readThisTime;
      buf += readThisTime;
//      Serial.print("read: ");
//      Serial.print(readThisTime);
//      Serial.print("  total: ");
//      Serial.println(bytesRead);
      if (bytesRead != 0) {
        start = millis();
      }
    }
  }
  return bytesRead;
}

bool handleRequest(WiFiClient & client, char * buf, int n) {
  char * urlloc;
  int urllen = findPath(buf,&urlloc);
  char urlval[urllen+1];
  memcpy(&urlval,urlloc,urllen);
  urlval[urllen] = 0;

  if (urllen == 0) {
    return false;
  }

  int contentLength = getContentLength(buf);
  int maxWait = 1000;

  if (strcmp(urlval,"/update") == 0) {
    Serial.println("Updating firmware!!");

//    char miniBuf[1000];
//    int bodyRead = 0;
//    while(true) {
//      int newRead = readBytes(client,miniBuf,1000,1000);
//      if (newRead == 0) break;
//      bodyRead += newRead;
//    }
//
//    Serial.print("Read: ");
//    Serial.println(bodyRead);

    loadFirmware(client,contentLength);
    sendErr(&client,"Update failed");
    return true;
  }

  //Serial.print("URL: ");
  //Serial.println(urlval);

  //Read body if it exists
  int bodyRead = 0;
  if (contentLength > 0) {
    while(client.connected() && !client.available() && maxWait--) delay(1);
    bodyRead = client.read((byte*)buf+n,contentLength);
    n += bodyRead;
    if (bodyRead != contentLength) {
      Serial.println("ERR: failed to read body!");
      return false;
    }
  }

  int val;
  if (strcmp(urlval,"/") == 0) {
    toggleStrip(!isPowerOn());

    char content[] = "<html><head><meta http-equiv='refresh' content='5'/></head><body>Refreshing page..</body></html>";
    sendHttp(&client,200,"OK","text/html",content);
  } else if (strcmp(urlval,"/description.xml") == 0) {
    SSDP.schema(client);
  } else if (strcmp(urlval,"/status") == 0) {
    sendStatus(&client);
  } else if (strcmp(urlval,"/power/on") == 0) {
    toggleStrip(true);
    sendOk(&client);
  } else if (strcmp(urlval,"/power/off") == 0) {
    toggleStrip(false);
    sendOk(&client);
  } else if (strcmp(urlval,"/power/toggle") == 0) {
    toggleStrip(!isPowerOn());
    sendOk(&client);
  } else if (strcmp(urlval,"/disconnect") == 0) {
    disconnect = true;
    sendOk(&client);
  } else if (strcmp(urlval,"/brightness") == 0) {
    bool success = getInteger(buf,"index",&val);
    if (!success) return false;
    config.brightness = val;
    saveConfiguration();
    sendOk(&client);
  } else if (strcmp(urlval,"/pattern/forget") == 0) {
    bool success = getInteger(buf,"index",&val);
    if (!success) return false;
    patternManager.deletePattern(val);
    sendOk(&client);
  } else if (strcmp(urlval,"/pattern/select") == 0) {
    bool success = getInteger(buf,"index",&val);
    if (!success) return false;
    selectPattern(val);
    sendOk(&client);
  } else if (strcmp(urlval,"/pattern/test") == 0 || strcmp(urlval,"/pattern/save") == 0) {
    bool isTestPattern = strcmp(urlval,"/pattern/test") == 0;
    char * ptr = strstr(buf,"\r\n\r\n");

    if (ptr == NULL) return false;
    ptr+=4;
    int bodysize = n - int(ptr-buf);

    //BE AWARE: word alignment seems to matter.. we're copying this to a different location to avoid pointer alignment issues
    if (bodysize <= sizeof(PatternManager::PatternMetadata)) return false;
    PatternManager::PatternMetadata pat;
    memcpy(&pat,ptr,sizeof(PatternManager::PatternMetadata));

    ptr += sizeof(PatternManager::PatternMetadata); //start of payload
    uint32_t remaining = n - int(ptr-buf);

    if (isTestPattern) {
      patternManager.saveTestPattern(&pat);
      patternManager.saveTestPatternBody(0,(byte*)ptr,remaining);
      patternManager.showTestPattern(true);
    } else {
      byte pattern = patternManager.saveLedPatternMetadata(&pat);
      patternManager.saveLedPatternBody(pattern,0,(byte*)ptr,remaining);
      selectPattern(pattern);
    }

    sendOk(&client);
  } else {
    char content[] = "Not Found";
    sendHttp(&client,404,"Not Found","text/plain",content);
  }
  return true;
}

int readUntil(WiFiClient * client, char * buffer, const char * search, long timeout) {
  long start = millis();
  int n = 0;
  int i = 0;
  int searchlen = strlen(search);
  while(client->connected()) {
    if (millis() - start > timeout) break;

    int b = client->read();
    if (b == -1) continue;
    start = millis();
    buffer[n++] = b;
    if (search[i] == b) {
      i++;
    } else {
      i = 0;
    }
    if (i >= searchlen) break;
  }

  return n;
}

const char* stristr(const char* str1, const char* str2 ) {
    const char* p1 = str1 ;
    const char* p2 = str2 ;
    const char* r = *p2 == 0 ? str1 : 0 ;

    while( *p1 != 0 && *p2 != 0 )
    {
        if( tolower( *p1 ) == tolower( *p2 ) )
        {
            if( r == 0 )
            {
                r = p1 ;
            }

            p2++ ;
        }
        else
        {
            p2 = str2 ;
            if( tolower( *p1 ) == tolower( *p2 ) )
            {
                r = p1 ;
                p2++ ;
            }
            else
            {
                r = 0 ;
            }
        }

        p1++ ;
    }

    return *p2 == 0 ? r : 0 ;
}

int getContentLength(const char * buf) {
  char search[] = "content-length:";
  const char * ptr = stristr(buf,search);
  if (ptr == NULL) return 0;
  ptr += strlen(search);
  while(ptr[0] == '\n' || ptr[0] == '\r' || ptr[0] == '\t' || ptr[0] == ' ') ptr++;
  return atoi(ptr);
}

char buf[2000];
void loop() {
  WiFiClient client = server.available();
  bool timedout = false;
  char ssid[] = "Steven's Castle";
  char pass[] = "Gizmo3151";
  memcpy(config.ssid,ssid,strlen(ssid)+1);
  memcpy(config.password,pass,strlen(pass)+1);
  while(true) {
    if (timedout || strlen(config.ssid) == 0) {
      //TODO handle unconfigured
      delay(10);
    } else {
      WiFi.mode(WIFI_STA);
      Serial.print("Connecting to ssid: ");
      Serial.println(config.ssid);
      WiFi.begin(config.ssid, config.password);

      unsigned long start = millis();
      unsigned long timeoutDuration = 15000;

      while (WiFi.status() != WL_CONNECTED) {
        if (millis() - start >= timeoutDuration) break;
        delay(1);
        //tick();
      }

      if (WiFi.status() == WL_CONNECTED) {
        Serial.print("Connected with IP:");
        Serial.println(WiFi.localIP());
      } else {
        break;
      }

      Serial.printf("Starting SSDP...\n");
      SSDP.setSchemaURL("description.xml");
      SSDP.setHTTPPort(80);
      SSDP.setName("Flickerstrip LED Strip");
      SSDP.setSerialNumber("12341234");
      SSDP.setURL("index.html");
      SSDP.setModelName("Flickerstrip LED Strip");
      SSDP.setModelNumber("fl_100");
      SSDP.setModelURL("http://flickerstrip.com");
      SSDP.setManufacturer("HomeAutomaton");
      SSDP.setManufacturerURL("http://homeautomaton.com");
      SSDP.begin();

      server.begin();

      int i = 0;
      while(WiFi.status() == WL_CONNECTED) {
        if (disconnect) {
          WiFi.disconnect();
          config.ssid[0] = 0;
          config.password[0] = 0;
          saveConfiguration();
          break;
        }

        tick();
        delay(1);
        WiFiClient client = server.available();
        if (!client) continue;

        int n = readUntil(&client,buf,"\r\n\r\n",1000);
        if (n == 0) {
          Serial.println("failed to read header!");
          client.stop();
          continue;
        }
        buf[n] = 0; //make sure we're terminated

        //Serial.println("======== HEADER =========");
        //Serial.println(buf);

        if (!handleRequest(client,buf,n)) {
          sendErr(&client,"Error handling request!");
        }

        int maxWait = 2000;
        while(client.connected() && maxWait--) {
          delay(1);
        }
        //Serial.print("DISC ");

        client.stop();
        //Serial.println();
      }
      Serial.println("disconnected from wifi");
    }
  }
}

/*
void handleRoot() {
  Serial.println("Serving root..");
  webserver.send(200, "text/html", "<html><head><meta http-equiv='refresh' content='5'/></head><body>Hello world</body></html>"); 
}

byte networkBuffer[2000];
void loop() {
  bool timedout = false;
  while(true) {
    if (timedout || strlen(config.ssid) == 0) {
      //TODO handle unconfigured
      delay(10);
    } else {
      WiFi.mode(WIFI_STA);
      Serial.print("Connecting to ssid: ");
      Serial.println(config.ssid);
      WiFi.begin(config.ssid, config.password);

      unsigned long start = millis();
      unsigned long timeoutDuration = 15000;

      while (WiFi.status() != WL_CONNECTED) {
        if (millis() - start >= timeoutDuration) break;
        delay(1);
        //tick();
      }

      Serial.print("Connected with IP:");
      Serial.println(WiFi.localIP());

      Serial.println("starting webserver");
      webserver.on("/", HTTP_GET, handleRoot);
      webserver.on("/description.xml", HTTP_GET, [](){
        Serial.println("responding to description request");
        SSDP.schema(webserver.client());
      });

      Serial.printf("Starting SSDP...\n");
      SSDP.setSchemaURL("description.xml");
      SSDP.setHTTPPort(80);
      SSDP.setName("Flickerstrip LED Strip");
      SSDP.setSerialNumber("12341234");
      SSDP.setURL("index.html");
      SSDP.setModelName("Flickerstrip LED Strip");
      SSDP.setModelNumber("fl_100");
      SSDP.setModelURL("http://flickerstrip.com");
      SSDP.setManufacturer("Reflowster");
      SSDP.setManufacturerURL("http://reflowster.com");
      SSDP.begin();

      webserver.begin();
      webserver.on("/status", HTTP_GET, [](){
        sendStatus();
      });
      webserver.on("/power/on", HTTP_GET, [](){
        toggleStrip(true);
        webserver.send(200, "application/json", "{\"response\":\"OK\"}"); 
      });
      webserver.on("/power/off", HTTP_GET, [](){
        toggleStrip(false);
        webserver.send(200, "application/json", "{\"response\":\"OK\"}"); 
      });
      webserver.on("/power/toggle", HTTP_GET, [](){
        toggleStrip(!isPowerOn());
        webserver.send(200, "application/json", "{\"response\":\"OK\"}"); 
      });

      while(WiFi.status() == WL_CONNECTED) {
        webserver.handleClient();
        tick();
        delay(0);
      }

      Serial.println("Disconnected!");
    }
  }
  bool timedout = false;
  while(true) {
    if (timedout || strlen(config.ssid) == 0) {
      //Handle configuration AP using a Captive Portal
      cpc.begin();
      //indicateBlueSlow();
      while(!cpc.hasConfiguration()) {
        cpc.tick();
        handleConnectedState();
        tick();
        delay(1);
      }
      //indicateStop();

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
      webserver.on("/", HTTP_GET, [](){
        webserver.sendHeader("Connection", "close");
        webserver.sendHeader("Access-Control-Allow-Origin", "*");
        webserver.send(200, "text/html", "<form method='POST' action='/update' enctype='multipart/form-data'><input type='file' name='update'><input type='submit' value='Update'></form>");

      });
      webserver.onFileUpload([](){
        if(webserver.uri() != "/update") return;
        HTTPUpload& upload = webserver.upload();
        if(upload.status == UPLOAD_FILE_START){
          Serial.setDebugOutput(true);
          WiFiUDP::stopAll();
          Serial.printf("Update: %s\n", upload.filename.c_str());
          uint32_t maxSketchSpace = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
          if(!Update.begin(maxSketchSpace)) { //start with max available size
            Update.printError(Serial);
          }
        } else if(upload.status == UPLOAD_FILE_WRITE){
          if(Update.write(upload.buf, upload.currentSize) != upload.currentSize){
            Update.printError(Serial);
          }
        } else if(upload.status == UPLOAD_FILE_END){
          if(Update.end(true)){ //true to set the size to the current progress
            Serial.printf("Update Success: %u\nRebooting...\n", upload.totalSize);
          } else {
            Update.printError(Serial);
          }
          Serial.setDebugOutput(false);
        }
        yield();
      });
      webserver.on("/update", HTTP_POST, [](){
        webserver.sendHeader("Connection", "close");
        webserver.sendHeader("Access-Control-Allow-Origin", "*");
        webserver.send(200, "text/plain", (Update.hasError())?"FAIL":"OK");
        ESP.restart();
      });
      webserver.begin();
      while(WiFi.status() == WL_CONNECTED) {
        handleConnectedState();
        webserver.handleClient();
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
*/
/*
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
*/
/////////////////////////////////////////////////////////////

void debugHex(const char *buf, int len) {
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

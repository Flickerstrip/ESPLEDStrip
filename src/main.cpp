// vim:ts=2 sw=2:

#include "Arduino.h"
#include <ESP8266WiFi.h>
#include <EEPROM.h>
#include <WiFiClient.h>
#include <DNSServer.h>
#include <ESP8266SSDP.h>
#include <WiFiServer.h>
#include <ESP8266WebServer.h>
#include <WiFiUdp.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>
#include <ArduinoJson.h>
#include "FastLED.h"

//Libraries maintained by Flickerstrip
#include <SoftwareSPI.h>
#include <FlashMemory.h>

#include "defines.h"
#include "LEDStrip.h"
#include "PatternMetadata.h"
#include "RunningPattern.h"
#include "util.h"
#include "version.h"
#include "networkutil.h"
#include "PatternManager.h"
#include "CaptivePortalConfigurator.h"

// use ESP.getResetReason() TODO

#define MAX_STRIP_LENGTH 750
FlashMemory flash(SPI_SCK,SPI_MOSI,SPI_MISO,MEM_CS);
LEDStrip strip;
PatternManager patternManager(&flash);
//CaptivePortalConfigurator cpc("esp8266confignetwork");
char defaultNetworkName[] = "Flickerstrip";
WiFiServer server(80);
WiFiUDP udp;
char buf[2000];

char mac[20];
bool disconnect = false;
bool reconnect = false;
long buttonDown = -1;
byte clicksTriggered = 0;
bool connecting = false;
long NETWORK_RETRY = 1000*60*3; //retry connection every 3 minutes

const byte BUTTON_UP = 1;
const byte BUTTON_DOWN = 0;

#define SSID_LENGTH 50
#define PASSWORD_LENGTH 50
#define NAME_LENGTH 50
bool debug = true;
struct Configuration {
  char ssid[SSID_LENGTH];
  char password[PASSWORD_LENGTH];
  char stripName[NAME_LENGTH];
  char groupName[NAME_LENGTH];
  byte selectedPattern;
  byte brightness;
  byte flags;
  int cycle;
  int stripLength;
  int stripStart;
  int stripEnd;
  int fadeDuration;
  char version[20];
};

bool powerOn = true;

const byte FLAG_CONFIGURED = 7;
const byte FLAG_REVERSED = 6;

const byte FLAG_CONFIGURED_CONFIGURED = 0;
const byte FLAG_CONFIGURED_UNCONFIGURED = 1;
const byte FLAG_REVERSED_FALSE = 0;
const byte FLAG_REVERSED_TRUE = 1;

struct PacketStructure {
	uint32_t type;
	uint32_t param1;
	uint32_t param2;
};

long lastSyncReceived;
long lastSyncSent;
long lastPingCheck;
int pingDelay;

Configuration config;

/////////////
bool accessPoint = false;
bool ignoreConfiguredNetwork = false;

byte heldTriggered = 0;
/////////////

//TODO create an H file? reorganize this all
void setup();
void buttonFix();
void createMacString();
void handleStartupHold();
void startEmergencyFirmwareMode();
void blinkCount(byte count, int on, int off);
void fillStrip(byte r, byte g, byte b);
void factoryReset();
void loadDefaultConfiguration();
bool loadConfiguration();
void saveConfiguration();
void setReversed(bool reversed);
bool isReversed();
void setNetwork(String ssid,String password);
void handleSerial();
void serialLine();
void selectPattern(byte pattern);
void sendStatus(WiFiClient * client);
void sendStatus(WiFiClient * client);
void patternTick();
void nextMode();
void nextModeWithToggle();
bool isPowerOn();
void toggleStrip(bool on);
void tick();
void buttonTick();
void setPulse(bool doPulse);
void setLed(byte n);
void setLedImpl(byte n);
void ledTick();
void broadcastUdp(char * buf, int len);
void syncTick();
void handleUdpPacket(char * charbuf, int len);
void loadFirmware(WiFiClient & client, uint32_t uploadSize);
int getPostParam(const char * content, const char * key, char * dst, int dstSize);
bool handleRequest(WiFiClient & client, char * buf, int n);
void handleWebClient(WiFiClient & client);
void startSSDP();
bool doConnect();
void forgetNetwork();
bool createAccessPoint();
void loop();

void setup() {
  Serial.begin(115200);
  pinMode(LED_STRIP,OUTPUT);
  pinMode(BUTTON_LED,OUTPUT);
  pinMode(BUTTON,INPUT);

  buttonFix();

  handleStartupHold();

  createMacString();

  Serial.println("\n\n");

  Serial.print("Flickerstrip Firmware Version: ");
  Serial.println(GIT_CURRENT_VERSION);

  //Load configuration returns false if configuration is not set
  if (!loadConfiguration()) {
    Serial.println("Initializing factory settings");
    patternManager.resetPatternsToDefault();
    loadDefaultConfiguration();
    saveConfiguration();
  }

  /*
  Serial.print("len: ");
  Serial.println(config.stripLength);
  Serial.print("start: ");
  Serial.println(config.stripStart);
  Serial.print("end: ");
  Serial.println(config.stripEnd);
  */

  //set up strip
  if (config.stripLength > MAX_STRIP_LENGTH) config.stripLength = MAX_STRIP_LENGTH;
  strip.setLength(config.stripLength);
  strip.setStart(config.stripStart);
  strip.setEnd(config.stripEnd);
  strip.setReverse((config.flags >> FLAG_REVERSED) & 0x1);
  patternManager.setTransitionDuration(config.fadeDuration);
  strip.begin(LED_STRIP);

  if (strcmp(config.version,GIT_CURRENT_VERSION) != 0) {
    Serial.println("Firmware version updated!");
    //TODO decide what we want to do here... run some kinda patcher?
    memcpy(config.version,GIT_CURRENT_VERSION,strlen(GIT_CURRENT_VERSION)+1);
  }

  patternManager.loadPatterns();
  Serial.print("Loaded patterns: ");
  Serial.println(patternManager.getPatternCount());
  lastSyncReceived = millis() + 3000;
  lastSyncSent = -1;
  lastPingCheck = -1;
  pingDelay = 0;
  patternManager.selectPattern(config.selectedPattern);

  digitalWrite(BUTTON_LED,1);
}

void buttonFix() {
  pinMode(BUTTON,OUTPUT); //Required for dan's flickerstrip.. TODO what's going on here?
  digitalWrite(BUTTON,1);
  pinMode(BUTTON,INPUT);
}

void createMacString() {
  uint8_t macAddr[WL_MAC_ADDR_LENGTH];
  WiFi.macAddress(macAddr);
  snprintf(mac,20,"%x:%x:%x:%x:%x:%x",macAddr[0],macAddr[1],macAddr[2],macAddr[3],macAddr[4],macAddr[5]);
}

void handleStartupHold() {
  long start = millis();
  digitalWrite(BUTTON_LED,BUTTON_LED_OFF);
  while(digitalRead(BUTTON) == BUTTON_DOWN) {
    long duration = millis() - start;
    if (heldTriggered == 0 && duration > 2000) {
      blinkCount(2,100,100);
      heldTriggered++;
    }

    if (heldTriggered == 1 && duration > 7000) {
      blinkCount(4,100,100);
      heldTriggered++;
    }

    if (heldTriggered == 2 && duration > 15000) {
      blinkCount(10,50,50);
      digitalWrite(BUTTON_LED,BUTTON_LED_ON);
      heldTriggered++;
      break;
    }

    buttonFix();
    delay(100);
  }

  if (heldTriggered == 1) {
    //Ignore the configured network for this boot (and set up in access point mode)
    ignoreConfiguredNetwork = true;
  }

  if (heldTriggered == 2) {
    factoryReset();
  }

  if (heldTriggered == 3) {
    //emergency firmware mode
    startEmergencyFirmwareMode();
  }
}

//This emergency code is used to recover if for some reason a firmware update has a bug in it
//USE EXTREME CARE WHEN EDITING THIS FUNCTION.
//AVOID USING NON-LIBRARY FUNCTION CALLS
void startEmergencyFirmwareMode() {
  ESP8266WebServer httpServer(80);
  ESP8266HTTPUpdateServer httpUpdater;

  IPAddress ip = IPAddress(192, 168, 1, 1);
  IPAddress netmask = IPAddress(255, 255, 255, 0);

  WiFi.disconnect();
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(ip,ip,netmask);
  WiFi.softAP("Flickerstrip");

  Serial.print("Created access point: Flickerstrip");
  Serial.print(" [");
  Serial.print(WiFi.softAPIP());
  Serial.println("]");

  httpUpdater.setup(&httpServer);
  httpServer.begin();

  Serial.printf("Booted into emergency firmware mode and ready to load firmware");

  while(true) {
    httpServer.handleClient();
    delay(1);
  }
}

void blinkCount(byte count, int on, int off) {
  for(int i=0; i<count; i++) {
    digitalWrite(BUTTON_LED,BUTTON_LED_ON);
    delay(on);
    digitalWrite(BUTTON_LED,BUTTON_LED_OFF);
    delay(off);
  }
}

void fillStrip(byte r, byte g, byte b) {
  for (int i=0; i<strip.getLength(); i++) {
    strip.setPixel(i,0,0,0);
  }
}

void factoryReset() {
  Serial.println("Clearing configuration and rebooting");
  EEPROM.begin(sizeof(Configuration));
  for (int i=0; i<sizeof(Configuration); i++) {
    EEPROM.write(i,0xff);
  }
  EEPROM.end();

  ESP.restart();
}

void loadDefaultConfiguration() {
  config.ssid[0] = 0;
  config.password[0] = 0;
  config.stripName[0] = 0;
  config.groupName[0] = 0;
  config.selectedPattern = 0;
  config.brightness = 10;
  config.cycle = 0;
  config.stripLength = 150;
  config.stripStart = 0;
  config.stripEnd = -1;
  config.flags = (FLAG_CONFIGURED_CONFIGURED << FLAG_CONFIGURED) & //Set configured bit
                 (FLAG_REVERSED_FALSE << FLAG_REVERSED); //Set reversed bit
  memcpy(config.version,GIT_CURRENT_VERSION,strlen(GIT_CURRENT_VERSION)+1);
}

bool loadConfiguration() {
  EEPROM.begin(sizeof(Configuration));
  for (int i=0; i<sizeof(Configuration); i++) {
    ((byte *)(&config))[i] = EEPROM.read(i);
  }
  EEPROM.end();

  return ((config.flags >> FLAG_CONFIGURED) & 1) == FLAG_CONFIGURED_CONFIGURED;
}

void saveConfiguration() {
  EEPROM.begin(sizeof(Configuration));
  for (int i=0; i<sizeof(Configuration); i++) {
    EEPROM.write(i,((byte *)(&config))[i]);

  }
  EEPROM.end();
}

void setReversed(bool reversed) {
  if (reversed) {
    config.flags |= 1 << FLAG_REVERSED;
  } else {
    config.flags &= ~(1 << FLAG_REVERSED);
  }

  strip.setReverse(isReversed());
}

bool isReversed() {
  return (config.flags >> FLAG_REVERSED) & 1;
}

void setNetwork(String ssid,String password) {
  ssid.toCharArray((char*)&config.ssid,50);
  password.toCharArray((char *)&config.password,50);
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
  if (strstr(serialBuffer,"ping") != NULL) {
    Serial.println("pong");
  } else if (strstr(serialBuffer,"mac") != NULL) {
    Serial.println(mac);
  } else if (strstr(serialBuffer,"dc") != NULL) {
    Serial.println("Resetting wireless configuration");
    forgetNetwork();
    ESP.restart();
  } else if (strstr(serialBuffer,"factory") != NULL) {
    factoryReset();
  } else if (strstr(serialBuffer,"reboot") != NULL) {
    ESP.restart();
  }
}

void selectPattern(byte pattern) {
  patternManager.showTestPattern(false);
  patternManager.selectPattern(pattern);
  config.selectedPattern = patternManager.getSelectedPattern();
  saveConfiguration();
}

char patternBuffer[1000];
void sendStatus(WiFiClient * client) {

  int n = snprintf(buf,2000,"{\
\"type\":\"status\",\
\"name\":\"%s\",\
\"group\":\"%s\",\
\"firmware\":\"%s\",\
\"power\":%d,\
\"mac\":\"%s\",\
\"selectedPattern\":%d,\
\"brightness\":%d,\
\"length\":%d,\
\"start\":%d,\
\"end\":%d,\
\"fade\":%d,\
\"reversed\":%d,\
\"cycle\":%d,\
\"uptime\":%d,\
\"memory\":{\"used\":%d,\"free\":%d,\"total\":%d},\
\"patterns\":",
  config.stripName,
  config.groupName,
  GIT_CURRENT_VERSION,
  isPowerOn(),
  mac,
  patternManager.getSelectedPattern(),
  config.brightness,
  config.stripLength,
  config.stripStart,
  config.stripEnd,
  config.fadeDuration,
  isReversed(),
  config.cycle,
  millis(),
  patternManager.getUsedBlocks(),
  patternManager.getAvailableBlocks(),
  patternManager.getTotalBlocks());

  int remaining = 2000 - n;
  char * ptr = buf + n;
  int patternBufferLength = patternManager.serializePatterns(ptr,remaining);
  ptr[patternBufferLength] = '}';
  ptr[patternBufferLength+1] = 0;

  sendHttp(client,200,"OK","application/json",buf);
}

/*
void sendStatus(WiFiClient * client) {
  StaticJsonBuffer<2000> jsonBuffer;

  JsonObject& root = jsonBuffer.createObject();
  root["type"] = "status";
  root["name"] = config.stripName;
  root["group"] = config.groupName;
  root["firmware"] = GIT_CURRENT_VERSION;
  root["power"] = isPowerOn();
  root["mac"] = mac;
  root["selectedPattern"] = patternManager.getSelectedPattern();
  root["brightness"] = config.brightness;

  JsonObject& mem = root.createNestedObject("memory");
  mem["used"] = patternManager.getUsedBlocks();
  mem["free"] = patternManager.getAvailableBlocks();
  mem["total"] = patternManager.getTotalBlocks();

  JsonArray& patterns = root.createNestedArray("patterns");
  patternManager.jsonPatterns(patterns);

  sendHttp(client,200,"OK",root);
}
*/

void patternTick() {
  byte brightness = (255*config.brightness)/100;
  strip.setBrightness(brightness);
  bool hasNewFrame = patternManager.loadNextFrame(&strip);
  if (hasNewFrame) {
    yield();
    ESP.wdtFeed();
    strip.show();
  }
}

void nextMode() {
  patternManager.showTestPattern(false);

  if(config.selectedPattern+1 >= patternManager.getPatternCount()) {
    selectPattern(0);
  } else {
    selectPattern(patternManager.getSelectedPattern()+1);
  }
}

void nextModeWithToggle() {
  if (!isPowerOn()) {
    toggleStrip(true);
  } else if(config.selectedPattern+1 >= patternManager.getPatternCount()) {
    selectPattern(0);
    toggleStrip(false);
  } else {
  }
}

bool isPowerOn() {
  return powerOn;
}

void toggleStrip(bool on) {
  powerOn = on;
}

int powerWasOn = true;
int rollOver = 0;
long lastSwitch = 0;
void tick() {
  ESP.wdtFeed();
  yield();
  long currentTime = millis();
  if (config.cycle != 0) {
    long rollOverTime = 1000*config.cycle;
    if (currentTime - lastSwitch > rollOverTime) {
      lastSwitch = currentTime;
      if (!patternManager.isTestPatternActive()) {
        nextMode();
      }
    }
  }

  handleSerial();
  if (isPowerOn()) {
    powerWasOn = true;
    patternTick();
  } else if (powerWasOn == true) {
    fillStrip(0,0,0);
    strip.show();
    powerWasOn = false;
  }

  if (connecting) {
    setPulse(true); 
  } else if (isPowerOn()) {
    setLed(0);
  } else {
    setLed(50);
  }

  syncTick();
  buttonTick();
  ledTick();
}

void buttonTick() {
  long currentTime = millis();
  long downTime = currentTime - buttonDown;
  if (digitalRead(BUTTON) == BUTTON_DOWN) {
    if (buttonDown == 0) {
      buttonDown = currentTime;
    } else if (buttonDown > 0 && downTime > 2000) {
      //very long press
      if (isPowerOn()) toggleStrip(false);
      buttonDown = -1;
    } 
  } else {
    if (buttonDown > 0) {
      if (downTime > 500) {
        //long press
      } else {
        //short press
        if (!isPowerOn()) {
          toggleStrip(true);
        } else {
          nextMode();
        }
      }
      buttonDown = -1;
    } else if (buttonDown == -1) {
      buttonDown = -currentTime; //debounce the button
    } else if (buttonDown < 0) {
      if (currentTime+buttonDown > 100) buttonDown = 0; //reset the button debounce
    }
  }
  buttonFix();
}

bool ledPulsing = false;
void setPulse(bool doPulse) {
  ledPulsing = doPulse;
}

void setLed(byte n) {
  setPulse(false);
  setLedImpl(n);
}

void setLedImpl(byte n) {
  analogWrite(BUTTON_LED,((int)n) << 2);
}

int pulseRate = 2000;
int pulseMin = 50;
int pulseMax = 255;
int tickSpacing = 50;
long lastTick = 0;
void ledTick() {
  if (!ledPulsing) return;
  int pulseRange = pulseMax - pulseMin;
  long ctime = millis();
  if (ctime - lastTick > tickSpacing) {
    lastTick = ctime;
    if (ctime % pulseRate < pulseRate/2) {
      setLedImpl(pulseMin + 2*((ctime % (pulseRate/2))*pulseRange)/pulseRate);
    } else {
      setLedImpl(pulseMax - 2*((ctime % (pulseRate/2))*pulseRange)/pulseRate);
    }
  }
}

void broadcastUdp(char * buf, int len) {
    Serial.print("SND: ");
    buf[len] = 0;
    Serial.println(buf);
    IPAddress ip = IPAddress(255, 255, 255, 255);
    udp.beginPacket(ip, 2836);
    udp.write(buf, len);
    udp.endPacket();
}

void syncTick() {
  if (strlen(config.groupName) == 0) return; //sync is disabled if group is empty
  //lastPingCheck = -1;
  //pingDelay = 0;
  long current = millis();
  if (current - lastSyncReceived > 5000 && current - lastSyncSent > 3000) {
    int n = 0;
    n = snprintf(buf,2000,"{'command':'sync','pattern':'%s','frame':%d,'group':'%s'}",patternManager.getActivePattern()->name,patternManager.getCurrentFrame(),config.groupName);
    broadcastUdp(buf,n);
    lastSyncSent = current;
    lastPingCheck = -1;
  }

  if (current - lastSyncReceived < 3000) {
    lastSyncSent = -1;
    if (current - lastPingCheck > 10000) {
      int n = 0;
      n = snprintf(buf,2000,"{'command':'ping','mac':'%s','group':'%s'}",mac,config.groupName);
      broadcastUdp(buf,n);
      lastPingCheck = current;
    }
  }
}

void handleUdpPacket(char * charbuf, int len) {
  Serial.print("REC: ");
  Serial.println(charbuf);
  StaticJsonBuffer<200> jsonBuffer;
  JsonObject& root = jsonBuffer.parseObject(charbuf);

  if (!root.containsKey("command")) {
    return;
  }

  if (root.containsKey("group")) {
    Serial.print("configured group:");
    Serial.println(config.groupName);

    Serial.print("group inc:");
    Serial.println(root["group"].asString());
    if (strcmp(config.groupName,root["group"]) != 0) {
      Serial.println("skipping");
      return;
    }

    if (strcmp(root["command"],"ping") == 0) {
      if (lastSyncSent != -1 && millis() - lastSyncSent < 3000) { //we're the master, we should respond to a ping
        int n = 0;
        char maccpy[20];
        strncpy(maccpy,root["mac"],20); //back up the macaddress
        n = snprintf(buf,2000,"{'command':'pingback','mac':'%s'}",maccpy); //respond with the pinger's mac address
        broadcastUdp(buf,n);
      }
      return; //we destroy the buffer.. so we should return
    }

    if (strcmp(root["command"],"pingback") == 0 && strcmp(root["mac"],mac) == 0) {
      pingDelay = (millis() - lastPingCheck)/2;
      Serial.print("got pingback.. ");
      Serial.println(pingDelay);
    }

    if (strcmp(root["command"],"sync") == 0) {
      int frame = 0;
      if (root.containsKey("frame")) {
        frame = root["frame"];
      }
      if (root.containsKey("pattern")) {
        if (strcmp(patternManager.getActivePattern()->name,root["pattern"]) != 0) {
          return;
        }
      }
      lastSyncReceived = millis();
      patternManager.syncToFrame(frame,pingDelay);
    }
  }

  if (strcmp(root["command"],"next") == 0) {
    nextMode();
  }

  if (strcmp(root["command"],"on") == 0) {
    toggleStrip(true);
  }

  if (strcmp(root["command"],"off") == 0) {
    toggleStrip(false);
  }

  if (strcmp(root["command"],"pattern") == 0) {
    if (root.containsKey("index")) {
      int select = root["index"];
      patternManager.selectPattern(select);
    }
    if (root.containsKey("name")) {
      patternManager.selectPattern(patternManager.getPatternIndexByName(root["name"].asString()));
    }
    patternManager.syncToFrame(0);
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

int getPostParam(const char * content, const char * key, char * dst, int dstSize) {
  char * ptr = strstr(content,key);
  if (ptr == NULL) return -1; //no key found
  ptr = strchr(ptr,'=');
  if (ptr == NULL) {
    dst[0] = 0;
    return 0; //key found, empty value found
  }

  int n = 0;
  content = ptr+1;
  while(*ptr++) {
    if (*ptr == 0 || *ptr == '&' || n >= dstSize) {
      break;
    }
    n++;
  }

  memcpy(dst,content,n);
  dst[n] = 0;
  return n;
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

  int val;
  if (strcmp(urlval,"/") == 0) {
    char content[] = "<html><head><meta name=viewport content=\"width=device-width, initial-scale=1\"></head><body><a href='/power/on'>Power On</a><br/><a href='/power/off'>Power Off</a></br><a href='/connect'>Configure WiFi</a></body></html>";
    sendHttp(&client,200,"OK","text/html",content);
  } else if (strcmp(urlval,"/description.xml") == 0) {
    SSDP.schema(client);
  } else if (strcmp(urlval,"/status") == 0) {
    sendStatus(&client);
  } else if (strcmp(urlval,"/config/name") == 0) {
    char body[contentLength+1];
    readBytes(client,(char*)&body,contentLength,1000);
    body[contentLength] = 0;
    StaticJsonBuffer<200> jsonBuffer;
    JsonObject& root = jsonBuffer.parseObject(body);

    memcpy(config.stripName,root.get<const char*>("name"),strlen(root.get<const char*>("name"))+1);
    saveConfiguration();
    sendStatus(&client);
  } else if (strcmp(urlval,"/config/group") == 0) {
    char body[contentLength+1];
    readBytes(client,(char*)&body,contentLength,1000);
    body[contentLength] = 0;
    StaticJsonBuffer<200> jsonBuffer;
    JsonObject& root = jsonBuffer.parseObject(body);

    memcpy(config.groupName,root.get<const char*>("name"),strlen(root.get<const char*>("name"))+1);
    saveConfiguration();
    sendStatus(&client);
  } else if (strcmp(urlval,"/config/cycle") == 0) {
    bool success = getInteger(buf,"value",&val);
    if (!success) return false;
    config.cycle = val;
    saveConfiguration();
    sendOk(&client);
  } else if (strcmp(urlval,"/config/fade") == 0) {
    bool success = getInteger(buf,"value",&val);
    if (!success) return false;
    config.fadeDuration = val;

    saveConfiguration();

    patternManager.setTransitionDuration(config.fadeDuration);

    sendOk(&client);
  } else if (strcmp(urlval,"/config/length") == 0) {
    bool success = getInteger(buf,"value",&val);
    if (!success) return false;

    config.stripLength = val;
    saveConfiguration();

    strip.setLength(config.stripLength);
    sendOk(&client);
  } else if (strcmp(urlval,"/config/start") == 0) {
    bool success = getInteger(buf,"value",&val);
    if (!success) return false;

    config.stripStart = val;
    saveConfiguration();

    strip.setStart(config.stripStart);
    sendOk(&client);
  } else if (strcmp(urlval,"/config/end") == 0) {
    bool success = getInteger(buf,"value",&val);
    if (!success) return false;

    config.stripEnd = val;
    saveConfiguration();

    strip.setEnd(config.stripEnd);
    sendOk(&client);
  } else if (strcmp(urlval,"/config/reversed") == 0) {
    bool success = getInteger(buf,"value",&val);
    if (!success) return false;

    setReversed(val == 1);
    saveConfiguration();

    sendOk(&client);
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
  } else if (strcmp(urlval,"/connect") == 0) {
    if (strstr(buf,"GET") != NULL) {
      sendHttp(&client,200,"OK","text/html",login_form);
    } else {
      char body[contentLength+1];
      readBytes(client,(char*)&body,contentLength,1000);
      body[contentLength] = 0;
      char * ptr = strtok (body,"=");
      while (ptr != NULL) {
        char key[strlen(ptr)+1];
        memcpy(&key,ptr,strlen(ptr)+1);
        ptr = strtok (NULL, "&");
        if (ptr == NULL) break;
        if (strcmp(key,"ssid") == 0) {
          urldecode(config.ssid,SSID_LENGTH,ptr);
        } else if (strcmp(key,"password") == 0) {
          urldecode(config.password,PASSWORD_LENGTH,ptr);
        }
        ptr = strtok (NULL, "=");
      }
      saveConfiguration();
      reconnect = true;
      sendHttp(&client,200,"OK","text/html",confirm_page);
    }
  } else if (strcmp(urlval,"/brightness") == 0) {
    bool success = getInteger(buf,"value",&val);
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

    //BE AWARE: word alignment seems to matter.. we're copying this to a different location to avoid pointer alignment issues
    PatternMetadata pat;
    int readSize = readBytes(client,(char*)&pat,sizeof(PatternMetadata),1000);
    if (readSize != sizeof(PatternMetadata)) {
      Serial.println("Error reading pattern metadata!");
      return false;
    }

    uint32_t remaining = contentLength - readSize;

    //TODO clean up the test vs save functionality (dedupe)
    if (isTestPattern) {
      patternManager.saveTestPattern(&pat);
      int page = 0;
      byte pagebuffer[0x100];
      while(remaining > 0) {
        int pageReadSize = 0x100;
        if (remaining < pageReadSize) pageReadSize = remaining;
        readSize = readBytes(client,(char*)&pagebuffer,0x100,1000);
        if (readSize != pageReadSize) {
          Serial.println("Short read!");
          return false;
        }
        remaining -= readSize;
        patternManager.saveTestPatternBody(page++,(byte*)&pagebuffer,0x100);
      }
      patternManager.showTestPattern(true);
    } else {
      byte pattern = patternManager.saveLedPatternMetadata(&pat);
      int page = 0;
      byte pagebuffer[0x100];
      while(remaining > 0) {
        int pageReadSize = 0x100;
        if (remaining < pageReadSize) pageReadSize = remaining;
        readSize = readBytes(client,(char*)&pagebuffer,0x100,1000);
        if (readSize != pageReadSize) {
          Serial.println("Short read!");
          return false;
        }
        remaining -= readSize;
        patternManager.saveLedPatternBody(pattern,page++,(byte*)&pagebuffer,0x100);
      }
      selectPattern(pattern);
    }

    client.flush();
    sendOk(&client);
  } else {
    char content[] = "Not Found";
    sendHttp(&client,404,"Not Found","text/plain",content);
  }
  Serial.flush();
  return true;
}

void handleWebClient(WiFiClient & client) {
  int n = readUntil(&client,buf,"\r\n\r\n",1000);
  if (n == 0) {
    Serial.println("failed to read header!");
    client.stop();
    return;
  }
  buf[n] = 0; //make sure we're terminated

  if (!handleRequest(client,buf,n)) {
    sendErr(&client,"Error handling request!");
  }

  int maxWait = 2000;
  while(client.connected() && maxWait--) {
    tick();
  }

  client.stop();
}

void startSSDP() {
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
}

bool doConnect() {
  Serial.print("Connecting to ssid: ");
  Serial.println(config.ssid);
  accessPoint = false;
  WiFi.disconnect();
  WiFi.mode(WIFI_STA);
  WiFi.begin(config.ssid, config.password);

  connecting = true;
  while (WiFi.status() == WL_DISCONNECTED) tick();
  connecting = false;

  if (WiFi.status() != WL_CONNECTED) return false;
  Serial.print("Connected with IP:");
  Serial.println(WiFi.localIP());
  return true;
}

void forgetNetwork() {
  WiFi.softAPdisconnect();
  WiFi.disconnect();
  config.ssid[0] = 0;
  config.password[0] = 0;
  saveConfiguration();
}

bool createAccessPoint() {
  accessPoint = true;
  IPAddress ip = IPAddress(192, 168, 1, 1);
  IPAddress netmask = IPAddress(255, 255, 255, 0);

  WiFi.disconnect();
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(ip,ip,netmask);
  WiFi.softAP(defaultNetworkName);

  Serial.print("Created access point: ");
  Serial.print(defaultNetworkName);
  Serial.print(" [");
  Serial.print(WiFi.softAPIP());
  Serial.println("]");
  return true;
}

void loop() {
  WiFiClient client = server.available();

  disconnect = false;
  reconnect = false;
  while(true) {
    int attempts = 0;
    while(!ignoreConfiguredNetwork && attempts <= 3 && strlen(config.ssid)) {
      attempts++;
      if (doConnect()) break;
    }

    //start own wifi network if we failed to connect above
    if (WiFi.status() != WL_CONNECTED) createAccessPoint();

    startSSDP();
    server.begin();
    //UDP seems to be unstable.. TODO investigate why
    udp.begin(2836);

    long lastRequest = millis();
    while(accessPoint || WiFi.status() == WL_CONNECTED) {
      //Attempt to reconnect to the parent network periodially if we haven't received any requests lately
      if (accessPoint && millis() - lastRequest > NETWORK_RETRY) reconnect = true;

      //Obey the disconnect command which forgets the configured network
      if (disconnect) return forgetNetwork();

      //  Reconnect
      if (reconnect) {
        WiFi.softAPdisconnect();
        WiFi.disconnect();
        Serial.println("reconnecting...");
        return;
      }

      tick();

      WiFiClient client = server.available();
      if (client) {
        lastRequest = millis();
        handleWebClient(client);
      }

      int udpLength = udp.parsePacket();
      if (udpLength > 0) {
        int readLength = udp.read(buf, 2000);
        buf[readLength] = 0;
        handleUdpPacket(buf,readLength);
      }
    }
  }
}


//Old pinout
//#define SPI_SCK 5
//#define SPI_MOSI 14
//#define SPI_MISO 16
//#define MEM_CS 4
//#define LED_STRIP 13

//New pinout
//#define SPI_SCK 12
//#define SPI_MOSI 16
//#define SPI_MISO 13
//#define MEM_CS 4
//#define LED_STRIP 14

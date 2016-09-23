#ifndef main_h
#define main_h

void setup();
void initializeConfiguration();
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

char mac[20];
char serialBuffer[100];
char serialIndex = 0;

#endif

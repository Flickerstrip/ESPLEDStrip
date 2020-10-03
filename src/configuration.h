#ifndef configuration_h
#define configuration_h

const byte FLAG_CONFIGURED = 7;
const byte FLAG_REVERSED = 6;
const byte FLAG_SELF_TEST = 5;

const byte FLAG_CONFIGURED_CONFIGURED = 0;
const byte FLAG_CONFIGURED_UNCONFIGURED = 1;
const byte FLAG_REVERSED_FALSE = 0;
const byte FLAG_REVERSED_TRUE = 1;
const byte FLAG_SELF_TEST_DONE = 0;
const byte FLAG_SELF_TEST_NEEDED = 1; //set by cradle

#define SSID_LENGTH 50
#define PASSWORD_LENGTH 50
#define NAME_LENGTH 50

struct Configuration {
  char version[20];
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
  byte failedBootCounter;
};

Configuration config;

#endif

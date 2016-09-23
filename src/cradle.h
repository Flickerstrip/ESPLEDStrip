#ifndef cradle_h
#define cradle_h

#include "main.h"
#include "bitutil.h"

bool detectCradle();
void handleCradleSerial();
void cradleSerialLine();

bool done;

struct Identity { 
    char uid[37];
    uint32_t batch;
    uint32_t unit;
};

Identity ident;

void handleCradle() {
    if (!detectCradle()) return;

    Serial.println("we're in cradle mode.. infinite loop time!");
    Serial.println("ready");
    pinMode(CRADLE_DONE_LED,OUTPUT);
    digitalWrite(CRADLE_DONE_LED,1);
    delay(10);

    initializeConfiguration();
    config.flags = bitset(config.flags,FLAG_SELF_TEST,FLAG_SELF_TEST_NEEDED);
    saveConfiguration();

    done = false;
    createMacString();
    while(!done) {
        handleCradleSerial();
        delay(1);
    }

    digitalWrite(CRADLE_DONE_LED,0);
    while(1) delay(100);
}

void cradleSerialLine() {
    if (strstr(serialBuffer,"ping") != NULL) {
        Serial.println("pong");
    } else if (strstr(serialBuffer,"mac") != NULL) {
        Serial.println(mac);
    } else if (strstr(serialBuffer,"identify") != NULL) {
        char * start = strchr(serialBuffer,':');
        start++;
        char * end = strchr(start,',');

        memcpy((char *)(&ident.uid),start,end-start);
        ident.uid[36] = 0;
        start = end + 1;
        end = strchr(start,',');
        end++;

        ident.batch = atoi(start);
        ident.unit = atoi(end);

        Serial.print("set identity: ");
        Serial.print(ident.uid);
        Serial.print(" ");
        Serial.print(ident.batch);
        Serial.print(" ");
        Serial.print(ident.unit);
        Serial.println();
    } else if (strstr(serialBuffer,"checkidentity") != NULL) {
        Serial.print("identity: ");
        Serial.print(ident.uid);
        Serial.print(" ");
        Serial.print(ident.batch);
        Serial.print(" ");
        Serial.print(ident.unit);
        Serial.println();
    } else if (strstr(serialBuffer,"done") != NULL) {
        done = true;
    }
}

void handleCradleSerial() {
  if (Serial.available()) {
    while(Serial.available()) {
      yield();
      char c = Serial.read();
      Serial.write(c);
      if (c == '\r') continue;
      if (c == '\n') {
        cradleSerialLine();
        serialIndex = 0;
      }
      serialBuffer[serialIndex++] = c;
      serialBuffer[serialIndex] = 0;
    }
  }
}

bool detectCradle() {
    pinMode(CRADLE_DETECT,INPUT_PULLUP);
    delay(10);
    bool isCradled = digitalRead(CRADLE_DETECT) == 0;

    pinMode(CRADLE_DETECT,OUTPUT); //TODO this is messy, we can do this because we know the cradle pin is being used as SPI_SCK and is an output.. but otherwise we're clobbering the pin mode
    delay(10);
    return isCradled;
}

#endif

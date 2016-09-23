#ifndef cradle_h
#define cradle_h

#include "main.h"
#include "bitutil.h"

bool detectCradle();
void handleCradleSerial();
void cradleSerialLine();
bool isOTPReady();
void handleOTPWrite(M25PXFlashMemory* flash);

bool done;

struct Identity { 
    char uid[37];
    uint32_t batch;
    uint32_t unit;
};

Identity ident;

//Note: While in cradle mode, the SPI bus is unavailable (eg. flash memory)
void handleCradle(M25PXFlashMemory* flash) {
    if (!detectCradle()) {
        if (isOTPReady()) handleOTPWrite(flash);
        return;
    }

    Serial.println("we're in cradle mode.. infinite loop time!");
    Serial.println("ready");
    pinMode(CRADLE_DONE_LED,OUTPUT);
    digitalWrite(CRADLE_DONE_LED,1);
    delay(10);

    //Mark the configuration as requiring the self-test
    initializeConfiguration();
    config.flags = bitset(config.flags,FLAG_SELF_TEST,FLAG_SELF_TEST_NEEDED);
    saveConfiguration();

    createMacString();

    if (isOTPReady()) digitalWrite(CRADLE_DONE_LED,0); //If the OTP is ready, we should display a green light

    while(1) {
        handleCradleSerial();
        delay(1);
    }
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
        EEPROM.begin(EEPROM_OTP+EEPROM_PAGE_SIZE);
        for (int i=0; i<sizeof(Identity); i++) {
            EEPROM.write(EEPROM_OTP+i,*((byte*)&ident+i));
        }
        EEPROM.end();
    } else if (strstr(serialBuffer,"checkidentity") != NULL) {
        //Load identity from EEPROM
        EEPROM.begin(EEPROM_OTP+EEPROM_PAGE_SIZE);
        for (int i=0; i<sizeof(Identity); i++) {
            ((byte*)&ident)[i] = EEPROM.read(EEPROM_OTP+i);
        }
        EEPROM.end();
        Serial.print("identity: ");
        Serial.print(ident.uid);
        Serial.print(" ");
        Serial.print(ident.batch);
        Serial.print(" ");
        Serial.print(ident.unit);
        Serial.println();
    } else if (strstr(serialBuffer,"done") != NULL) {
        digitalWrite(CRADLE_DONE_LED,0); //Cradle computer can tell us when we're done, display the light
    }
    serialLine();
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

bool isOTPReady() {
    EEPROM.begin(EEPROM_OTP+EEPROM_PAGE_SIZE);
    for (int i=0; i<sizeof(Identity); i++) {
        ((byte*)&ident)[i] = EEPROM.read(EEPROM_OTP+i);
    }
    EEPROM.end();

    return ((byte*)&ident)[0] != 0xff;
}

void handleOTPWrite(M25PXFlashMemory* flash) { //Only executes if cradle is not active
    Serial.println("\n\n");

    byte otpbuf[65];

    flash->readOTP(0,(byte*)&otpbuf,65);
    if (otpbuf[0] != 0xff) {
        Serial.println("OTP already written.. but EEPROM contains OTP. Clearing EEPROM OTP");
    } else {
        Serial.println("OTP found in EEPROM, writing it to the OTP");

        //Program the OTP
        EEPROM.begin(EEPROM_OTP+EEPROM_PAGE_SIZE);
        for (int i=0; i<sizeof(Identity); i++) otpbuf[i] = EEPROM.read(EEPROM_OTP+i);
        EEPROM.end();

        flash->programOTP(0,(byte*)&otpbuf,sizeof(Identity));

        //Freeze the OTP
        flash->freezeOTP();
    }

    //Clear the OTP from EEPROM
    EEPROM.begin(EEPROM_OTP+EEPROM_PAGE_SIZE);
    for (int i=0; i<sizeof(Identity); i++) EEPROM.write(EEPROM_OTP+i,0xff);
    EEPROM.end();
}

#endif

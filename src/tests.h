#include <SoftwareSPI.h>
#include <M25PXFlashMemory.h>
#include "LEDStrip.h"
#include "defines.h"

void testButton();
void testStrip(LEDStrip* strip);
bool testMemory(M25PXFlashMemory* flash);

void testAll(M25PXFlashMemory* flash, LEDStrip* strip) {
    strip->setLength(10);
    strip->setBrightness(50);

    Serial.println("\n");
    Serial.println("Running self test..");

    Serial.println("Testing Button");
    testButton();

    Serial.println("Testing Strip");
    testStrip(strip);
    strip->clear();
    strip->show();

    Serial.println("Testing Memory");
    if (testMemory(flash)) {
        Serial.println("Memory test passed");
        strip->clear();
        for (int i=0; i<10; i++) strip->setPixel(i,0,255,0);
        strip->show();
    } else {
        Serial.println("Memory test failed");
        strip->clear();
        for (int i=0; i<10; i++) strip->setPixel(i,255,0,0);
        strip->show();
    }

    Serial.println("Tests complete!");
}

void testButton() {
    while(digitalRead(BUTTON) == BUTTON_DOWN) yield();

    int i=0;
    while(1) {
        if (i%2 == 0) {
            digitalWrite(BUTTON_LED,BUTTON_LED_OFF);
        } else {
            digitalWrite(BUTTON_LED,BUTTON_LED_ON);
        }
        i++;
        delay(50);
        if (digitalRead(BUTTON) == BUTTON_DOWN) {
            while(digitalRead(BUTTON) == BUTTON_DOWN) yield();
            break;
        }
    }

    digitalWrite(BUTTON_LED,BUTTON_LED_OFF);
}

void testStrip(LEDStrip* strip) {
    while(digitalRead(BUTTON) == BUTTON_DOWN) yield();

    int i=0;
    while(1) {
        int pixel = i/4;
        int color = i % 4;

        strip->clear();
        strip->setPixel(pixel,color == 0 || color == 1 ? 255 : 0,color == 0 || color == 2 ? 255 : 0,color == 0 || color == 3 ? 255 : 0);
        strip->show();

        delay(50);
        if (i++ > 30) i=0;

        if (digitalRead(BUTTON) == BUTTON_DOWN) {
            while(digitalRead(BUTTON) == BUTTON_DOWN);
            break;
        }
    }
}

bool fail(char * msg) {
    Serial.print("FAIL: ");
    Serial.println(msg);
    return false;
}

bool testMemory(M25PXFlashMemory* flash) {
    if (flash->readStatus() != 0) return fail("ready initially");
    flash->enableWrite();
    if (flash->readStatus() != 2) return fail("write mode");
    flash->disableWrite();
    if (flash->readStatus() != 0) return fail("disabled write");

    flash->eraseSubsector(0);
    if (flash->readStatus() != 3) return fail("busy erasing subsector");
    while(flash->isBusy()) delay(1);
    if (flash->readStatus() != 0) return fail("ready after erase");

    flash->programByte(0x0000,42);
    flash->programByte(0x0001,7);
    flash->programByte(0x00010,1);

    if (flash->readByte(0x0000) != 42) return fail("read 42");
    if (flash->readByte(0x0001) != 7) return fail("read 7");
    if (flash->readByte(0x00010) != 1) return fail("read 1");

    flash->eraseSubsector(0);

    if (flash->readByte(0x0000) != 0xff) return fail("read blank");
    if (flash->readByte(0x0001) != 0xff) return fail("read blank");
    if (flash->readByte(0x00010) != 0xff) return fail("read blank");

    return true;
}


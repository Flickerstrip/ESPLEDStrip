#include <SoftwareSPI.h>
#include <M25PXFlashMemory.h>
#include "LEDStrip.h"

void testAll(M25PXFlashMemory flash, LEDStrip strip) {
    Serial.print("Status: ");
    Serial.println(flash.readStatus());
    flash.enableWrite();
    Serial.print("Status: ");
    Serial.println(flash.readStatus());
    flash.disableWrite();
    Serial.print("Status: ");
    Serial.println(flash.readStatus());

    Serial.println("erasing subsector 0");
    flash.eraseSubsector(0);
    Serial.print("Status: ");
    Serial.println(flash.readStatus());
    Serial.print("reading byte 0: ");
    Serial.println(flash.readByte(0));

    Serial.println("programmign byte");
    flash.programByte(0,42);

    Serial.print("reading byte 0: ");
    Serial.println(flash.readByte(0));
}

void testMemory(M25PXFlashMemory flash) {

}

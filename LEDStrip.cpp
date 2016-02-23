// vim:ts=2 sw=2:
#include "LEDStrip.h"

LEDStrip::LEDStrip() {
  this->length = 1;
  this->ledBuffer = NULL;
}

void LEDStrip::begin(const uint8_t pin) {
  this->ledBuffer = new CRGB[this->length];

  this->controller = &FastLED.addLeds<WS2812B, LED_STRIP, GRB>(this->ledBuffer, this->length);
}

void LEDStrip::setLength(int length) {
  this->length = length;
  if (this->ledBuffer != NULL) {
    delete [] this->ledBuffer;
  }
}

int LEDStrip::getLength() {
  return this->length;
}

void LEDStrip::setPixel(int i, byte r, byte g, byte b) {
  this->ledBuffer[i] = CRGB( r,g,b );
}

void LEDStrip::setBrightness(byte brightness) {
  this->brightness = brightness;
}

void LEDStrip::show() {
  this->controller->showLeds(this->brightness);
}

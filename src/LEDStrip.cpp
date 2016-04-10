// vim:ts=2 sw=2:
#include "LEDStrip.h"

LEDStrip::LEDStrip() {
  this->length = 1;
  this->ledBuffer = NULL;
  this->controller = NULL;
  this->start = 0;
  this->end = 1;
  this->reverse = false;
}

void LEDStrip::begin(const uint8_t pin) {
  Serial.println("ledstrip begin");
  this->ledBuffer = new CRGB[this->length];

  this->controller = &FastLED.addLeds<WS2812B, LED_STRIP, GRB>(this->ledBuffer, this->length);
}

void LEDStrip::setLength(int length) {
  this->clear();
  this->show();
  this->length = length;
  if (this->ledBuffer != NULL) {
    delete [] this->ledBuffer;
  }
  this->clear();
}

void LEDStrip::setStart(int start) {
  this->clear();
  this->show();
  this->start = start;
}

void LEDStrip::setEnd(int end) {
  this->clear();
  this->show();
  if (end == -1) end = this->length;
  this->end = end;
}

void LEDStrip::setReverse(bool reverse) {
  this->reverse = reverse;
}

int LEDStrip::getLength() {
  return this->length;
}

void LEDStrip::clear() {
  if (this->ledBuffer == NULL) return;
  for (int i=0; i<this->length; i++) {
    this->ledBuffer[i] = CRGB( 0,0,0 );
  }
}

void LEDStrip::setPixel(int i, byte r, byte g, byte b) {
  int index = i + this->start;
  if (index > this->end-1) return;

  if (this->reverse) {
    index = this->end - i - 1;
    if (index < this->start) return;
  }

  /*
  Serial.print("SET: ");
  Serial.print(i);
  Serial.print(" ");
  Serial.print(r);
  Serial.print(" ");
  Serial.print(g);
  Serial.print(" ");
  Serial.print(b);
  Serial.println();
  */
  this->ledBuffer[index] = CRGB( r,g,b );
}

void LEDStrip::addPixel(int i, byte r, byte g, byte b) {
  int index = i + this->start;
  if (index > this->end-1) return;

  if (this->reverse) {
    index = this->end - i - 1;
    if (index < this->start) return;
  }

  this->ledBuffer[index].r = this->ledBuffer[index].r + r;
  this->ledBuffer[index].g = this->ledBuffer[index].g + g;
  this->ledBuffer[index].b = this->ledBuffer[index].b + b;
}

void LEDStrip::setBrightness(byte brightness) {
  this->brightness = brightness;
}

void LEDStrip::show() {
  if (this->controller == NULL) return;
  this->controller->showLeds(this->brightness);
}

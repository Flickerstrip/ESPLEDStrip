// vim:ts=4 sw=4:
#include "LEDStrip.h"

LEDStrip::LEDStrip() {
  this->length = 1;
  this->ledBuffer = NULL;
  this->controller = NULL;
  this->start = -1;
  this->end = -1;
  this->reverse = false;
}

void LEDStrip::begin(const uint8_t pin) {
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
  this->ledBuffer = new CRGB[this->length];
  this->clear();
  this->controller->setLeds(this->ledBuffer,this->length);
}

void LEDStrip::setStart(int start) {
  this->start = start;
}

void LEDStrip::setEnd(int end) {
  this->end = end;
}

void LEDStrip::setReverse(bool reverse) {
  this->reverse = reverse;
}

int LEDStrip::getLength() {
  return this->length;
}

int LEDStrip::getStart() {
  int start = this->start;
  if (start == -1) start = 0;
  return start;
}

int LEDStrip::getEnd() {
  int end = this->end;
  if (end == -1) end = this->length;
  return end;
}

void LEDStrip::clear() {
  if (this->ledBuffer == NULL) return;
  for (int i=0; i<this->length; i++) {
    this->ledBuffer[i] = CRGB( 0,0,0 );
  }
}

void LEDStrip::setPixel(int i, byte r, byte g, byte b) {
  int index = i + this->getStart();
  if (index > this->getEnd()-1) return;

  if (this->reverse) {
    index = this->getEnd() - i - 1;
    if (index < this->getStart()) return;
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
  int index = i + this->getStart();
  if (index > this->getEnd()-1) return;

  if (this->reverse) {
    index = this->getEnd() - i - 1;
    if (index < this->getStart()) return;
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

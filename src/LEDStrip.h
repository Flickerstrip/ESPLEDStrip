// vim:ts=2 sw=2:
#ifndef LEDStrip_h
#define LEDStrip_h

#if defined(ARDUINO) && ARDUINO >= 100
#include "Arduino.h"
#else
#include "WProgram.h"
#endif

#include "defines.h"
#include "FastLED.h"

class LEDStrip {
public:
  LEDStrip();

  void begin(const uint8_t pin);
  void setLength(int length);
  void setStart(int start);
  void setEnd(int end);
  void setReverse(bool reverse);
  int getLength();
  int getStart();
  int getEnd();

  void clear();
  void setPixel(int i, byte r, byte g, byte b);
  void addPixel(int i, byte r, byte g, byte b);
  void setBrightness(byte brightness);
  void show();

private:
  CLEDController * controller;
  CRGB * ledBuffer;
  int length;
  byte brightness;
  int start;
  int end;
  bool reverse;
};

#endif



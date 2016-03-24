// vim:ts=2 sw=2:
#ifndef RunningPattern_h
#define RunningPattern_h

#if defined(ARDUINO) && ARDUINO >= 100
#include "Arduino.h"
#else
#include "WProgram.h"
#endif

#include "LEDStrip.h"
#include "PatternMetadata.h"
#include <FlashMemory.h>

class RunningPattern {
public:
  RunningPattern();
  RunningPattern(PatternMetadata * pattern, char * buf);

  void syncToFrame(int frame,int pingDelay = 0);
  bool needsUpdate();
  void loadNextFrame(LEDStrip * strip, FlashMemory * flash, float multiplier);

  int getCurrentFrame();
  RunningPattern& operator=(const RunningPattern& a);

private:
  PatternMetadata * pattern;

  char * buf;

  int currentFrame = 0;
  long lastFrameTime = 0;
};

#endif




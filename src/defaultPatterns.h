// vim:ts=4 sw=4 nowrap:
#ifndef DefaultPatterns_h
#define DefaultPatterns_h

struct PatternDefinition {
  char * name;
  int frames;
  int pixels;
  int fps;
  int size;
  byte * data;
};

#include "patterns/CarnivalRainbow.h"
#include "patterns/ChasingRainbow15.h"
#include "patterns/Cracksauce.h"
#include "patterns/EmberCrawl.h"
#include "patterns/FireSparkle.h"
#include "patterns/LEDTotemColorSplash.h"
#include "patterns/LEDTotemRainbowSpread.h"
#include "patterns/LEDTotemSpiralBounce.h"
#include "patterns/PanShift.h"
#include "patterns/PoliceLights-QuinteFlash.h"
#include "patterns/RainbowChase.h"
#include "patterns/Sam-JulieSetting2.h"
#include "patterns/SomeFireworks.h"

#endif

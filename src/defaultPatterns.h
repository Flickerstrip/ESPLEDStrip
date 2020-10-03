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


#include "patterns/01LEDTotemColorSplash.h"
#include "patterns/03LEDTotemContrastSpread.h"
#include "patterns/07LEDTotemRainbowChase2.h"
#include "patterns/08LEDTotemRainbowFade.h"
#include "patterns/09LEDTotemRainbowRicoshet.h"
#include "patterns/10LEDTotemSpiralBounce.h"
#include "patterns/11LEDTotemRainbowRipple.h"
#include "patterns/12LEDTotemSectionalColorFade.h"
#include "patterns/13LEDTotemRainbowSpread.h"
#include "patterns/14RGBSpread.h"
#include "patterns/15RainbowChase.h"
#include "patterns/17LEDTotemColorPinch.h"
#include "patterns/LEDTotemColorSpread.h"
#include "patterns/LEDTotemPurpleColorSpread.h"
#include "patterns/LEDTotemPurpleEmberCrawl.h"

#endif

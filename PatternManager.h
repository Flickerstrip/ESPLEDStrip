// vim:ts=2 sw=2:
#ifndef PatternManager_h
#define PatternManager_h

#if defined(ARDUINO) && ARDUINO >= 100
#include "Arduino.h"
#else
#include "WProgram.h"
#endif

#include <Adafruit_NeoPixel.h>
#include <FlashMemory.h>

class PatternManager {
public:
  PatternManager(FlashMemory * mem);

  struct PatternMetadata {
    char name[16];
    uint32_t address;
    uint32_t len;
    uint16_t frames;
    uint8_t flags;
    uint8_t fps;
  };

  void loadPatterns();
  void resetPatternsToDefault();
  void clearPatterns();
  void deletePattern(byte n);
  void selectPattern(byte n);
  uint32_t findInsertLocation(uint32_t len);
  byte saveLedPatternMetadata(PatternMetadata * pat);
  void saveLedPatternBody(int pattern, uint32_t patternStartPage, byte * payload, uint32_t len);

  void saveTestPattern(PatternMetadata * pat);
  void saveTestPatternBody(uint32_t patternStartPage, byte * payload, uint32_t len);
  void showTestPattern(bool show);

  int getTotalBlocks();
  int getUsedBlocks();
  int getAvailableBlocks();

  int getPatternCount();
  int getSelectedPattern();
  PatternMetadata * getActivePattern();

  bool loadNextFrame(Adafruit_NeoPixel &strip);
  int serializePatterns(char * buf, int len);

private:
  const static int NUM_PAGES = 4096;
  const static int MAX_PATTERNS = 17;

  FlashMemory * flash;

  char buf[1000];
  PatternMetadata patterns[MAX_PATTERNS];
  PatternMetadata testPattern;
  bool testPatternActive;

  int selectedPattern;
  int patternCount;
  int lastSavedPattern;

  int currentFrame = 0;
  long lastFrameTime = 0;
};

#endif



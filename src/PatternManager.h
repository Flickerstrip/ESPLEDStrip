// vim:ts=2 sw=2:
#ifndef PatternManager_h
#define PatternManager_h

#if defined(ARDUINO) && ARDUINO >= 100
#include "Arduino.h"
#else
#include "WProgram.h"
#endif

#include "LEDStrip.h"
#include "RunningPattern.h"
#include "PatternMetadata.h"
#include <FlashMemory.h>
#include <ArduinoJson.h>

#define MAXIMUM_PATTERN_BUFFER 450

class PatternManager {
public:
  PatternManager(FlashMemory * mem);

  void loadPatterns();
  void resetPatternsToDefault();
  void clearPatterns();
  void deletePattern(byte n);
  void selectPattern(byte n);
  void setTransitionDuration(int duration);
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
  int getCurrentFrame();
  int getPatternIndexByName(const char * name);
  bool isTestPatternActive();
  PatternMetadata * getActivePattern();
  PatternMetadata * getPrevPattern();

  void syncToFrame(int frame,int pingDelay = 0);
  bool loadNextFrame(LEDStrip * strip);
  int serializePatterns(char * buf, int len);
  void jsonPatterns(JsonArray& json);

private:
  const static int NUM_PAGES = 4096;
  const static int MAX_PATTERNS = 17;

  FlashMemory * flash;

  char buf[1000];
  PatternMetadata patterns[MAX_PATTERNS];
  PatternMetadata testPattern;
  bool testPatternActive;

  int prev_selectedPattern;
  int selectedPattern;
  RunningPattern prev;
  RunningPattern current;

  int patternCount;
  int lastSavedPattern;

  // Transition variables
  long patternTransitionTime;
  int transitionDuration;
  long lastFrame;
};

#endif



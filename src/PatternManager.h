// vim:ts=4 sw=4:
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
#include <EEPROM.h>
#include <M25PXFlashMemory.h>
#include <ArduinoJson.h>
#include "EEPROMLayout.h"

void debugHex(const char *buf, int len);

class PatternManager {
public:
    PatternManager(M25PXFlashMemory * mem);

    void loadPatterns();
    void echoPatternTable();
    void resetPatternsToDefault();
    void clearPatterns();
    void deletePatternByIndex(byte n);
    void deletePatternById(uint8_t patternId);
    void selectPatternByIndex(byte n);
    void selectPatternById(uint8_t patternId);
    bool isValidPatternId(uint8_t patternId);
    void setTransitionDuration(int duration);
    bool hasTestPattern();
    uint32_t findInsertLocation(uint32_t len);
    uint8_t saveLedPatternMetadata(PatternMetadata * pat, bool previewPattern);
    void saveLedPatternBody(uint8_t patternId, uint32_t patternStartPage, byte * payload, uint32_t len);

    int getTotalBlocks();
    int getUsedBlocks();
    int getAvailableBlocks();

    int getPatternCount();
    byte getSelectedId();
    byte getSelectedIndex();
    int getCurrentFrame();
    int getPatternIndexByName(const char * name);
    bool isTestPatternActive();
    PatternMetadata * getActivePattern();
    PatternMetadata * getPrevPattern();

    void syncToFrame(int frame,int pingDelay = 0);
    void freezeFrame(int frame);
    bool loadNextFrame(LEDStrip * strip);
    int serializePatterns(char * buf, int len);
    void jsonPatterns(JsonArray& json);

private:
    const static int NUM_SUBSECTORS = 250; //TODO update me for 16M flash

    M25PXFlashMemory * flash;

    char buf[1000];
    PatternReference patternsByAddress[MAX_PATTERNS];
    PatternMetadata patterns[MAX_PATTERNS];

    byte prev_selectedPattern;
    byte selectedPattern;
    RunningPattern prev;
    RunningPattern current;

    int patternCount;
    int lastSavedPattern;

    int freezeFrameIndex;

    // Transition variables
    long patternTransitionTime;
    int transitionDuration;
    long lastFrame;

    uint8_t findPatternById(uint8_t patternId);
    int insertPatternReference(PatternReference * ref);
    void saveReferenceTable();
    uint8_t createId();
};

#endif



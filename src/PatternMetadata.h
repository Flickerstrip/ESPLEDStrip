// vim:ts=4 sw=4:
#ifndef PatternMetadata_h
#define PatternMetadata_h

#define PATTERN_NAME_LENGTH 50

struct PatternMetadata {
  char name[PATTERN_NAME_LENGTH];
  uint32_t address;
  uint32_t len;
  uint16_t frames;
  uint8_t flags;
  uint8_t fps;
};

struct PatternReference {
  uint32_t address;
  uint32_t len;
};

const static int MAX_PATTERNS = 100;
#endif

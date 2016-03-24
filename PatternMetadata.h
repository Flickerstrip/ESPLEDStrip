// vim:ts=2 sw=2:
#ifndef PatternMetadata_h
#define PatternMetadata_h

struct PatternMetadata {
  char name[16];
  uint32_t address;
  uint32_t len;
  uint16_t frames;
  uint8_t flags;
  uint8_t fps;
};

#endif

// vim:ts=4 sw=4:
#ifndef EEPROMLayout_h
#define EEPROMLayout_h

#define EEPROM_SIZE 4096

#define EEPROM_PAGE_SIZE 0x100

#define EEPROM_OTP 0x200
#define EEPROM_TEST_PATTERN 0x300
#define EEPROM_PATTERNS_START 0x400
#define EEPROM_PATTERNS_PAGES 4

// EEPROM MEMORY MAP:
// [  0x000 - 0x0ff  ] Configuration (actual size: ~248 bytes)
// [  0x100 - 0x1ff  ] Configuration
// [  0x200 - 0x2ff  ] OTP Region (actual size: 64 bytes)
// [  0x300 - 0x3ff  ] Test pattern metadata (TODO: FIX THIS, make test pattern just pattern at index 0 or something..)
// [  0x400 - 0x4ff  ] Pattern References  (each is 8 bytes.. so 8*MAX_PATTERNS = 800)
// [  0x500 - 0x5ff  ] Pattern References  (each is 8 bytes.. so 8*MAX_PATTERNS = 800)
// [  0x600 - 0x6ff  ] Pattern References  (each is 8 bytes.. so 8*MAX_PATTERNS = 800)
// [  0x700 - 0x7ff  ] Pattern References  (each is 8 bytes.. so 8*MAX_PATTERNS = 800)
// [  0x800 - 0x8ff  ] UNUSED
// [  0x900 - 0x9ff  ] UNUSED
// [  0xa00 - 0xaff  ] UNUSED
// [  0xb00 - 0xbff  ] UNUSED
// [  0xc00 - 0xcff  ] UNUSED
// [  0xd00 - 0xdff  ] UNUSED
// [  0xe00 - 0xeff  ] UNUSED
// [  0xf00 - 0xfff  ] UNUSED

#endif

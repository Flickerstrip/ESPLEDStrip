#ifndef bitutil_h
#define bitutil_h

int bitset(int number, byte bit, bool value) {
  number ^= (-value ^ number) & (1 << bit);
  return number;
}

bool checkbit(int number, byte bit) {
  return (number >> bit) & 1;
}

#endif

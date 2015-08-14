// vim:ts=2 sw=2:
#include "PatternManager.h"

PatternManager::PatternManager(FlashMemory * mem) {
  this->flash = mem;
  this->patternCount = 0;
  this->lastSavedPattern = -1;
}

void PatternManager::loadPatterns() {
  this->flash->readBytes(0x100,(byte *)this->patterns,sizeof(PatternMetadata)*MAX_PATTERNS);
  PatternMetadata * ptr = this->patterns;
  for (int i=0; i<MAX_PATTERNS; i++) {
    if (ptr->address == 0xffffffff) {
      this->patternCount = i;
      return;
    }
    ptr++;
  }
  this->patternCount = this->MAX_PATTERNS;
}

void PatternManager::resetPatternsToDefault() {
  this->clearPatterns();

  Serial.println("generating new pattern");
  PatternMetadata newpat;
  char foo[] = "TwoBlink";
  memcpy(newpat.name, foo, strlen(foo)+1);
  newpat.len = 10*3*2;
  newpat.frames = 2;
  newpat.flags = 0;
  newpat.fps = 1;
  for (int i=0; i<10; i++) {
    this->buf[i*3+0] = 255;
    this->buf[i*3+1] = 255;
    this->buf[i*3+2] = 255;
  }
  for (int i=0; i<10; i++) {
    this->buf[30+i*3+0] = 255;
    this->buf[30+i*3+1] = 0;
    this->buf[30+i*3+2] = 0;
  }

  byte patindex = this->saveLedPatternMetadata(&newpat);
  this->saveLedPatternBody(patindex,0,(byte*)this->buf,10*3*2);
}

void PatternManager::clearPatterns() {
  this->flash->erasePage(0x100);
  this->flash->erasePage(0x200);
  this->patternCount = 0;
  this->selectedPattern = 0xff;
  lastSavedPattern = -1;
}

void PatternManager::selectPattern(byte n) {
    this->selectedPattern = n;
    if (this->selectedPattern == 0xff) {
        Serial.println("Mode set to 0xff, LEDs off");
    } else {
        PatternMetadata * pat = &this->patterns[n];
        Serial.print("Selected pattern: ");
        Serial.println(pat->name);
    }
    
    this->currentFrame = 0;
    this->lastFrameTime = 0;
}

void PatternManager::deletePattern(byte n) {
  this->patternCount--;
  for (int i=n; i<this->patternCount; i++) {
    memcpy(&this->patterns[i],&this->patterns[i+1],sizeof(PatternMetadata));
  }
  if (n <= this->selectedPattern) {
    selectPattern(this->selectedPattern-1);
  }

  byte * ptr = (byte*)(&this->patterns[this->patternCount]);
  for (int i=0; i<sizeof(PatternMetadata); i++) {
    ptr[i] = 0xff;
  }
  this->flash->writeBytes(0x100+sizeof(PatternMetadata)*n,(byte *)(&this->patterns[n]),(this->patternCount-n+1)*sizeof(PatternMetadata));
}

uint32_t PatternManager::findInsertLocation(uint32_t len) {
  for (int i=0; i<this->patternCount; i++) {
    uint32_t firstAvailablePage = ((this->patterns[i].address + this->patterns[i].len) & 0xffffff00) + 0x100;

    if (i == this->patternCount - 1) return i+1; //last pattern

    uint32_t spaceAfter = this->patterns[i+1].address - firstAvailablePage;
    if (spaceAfter > len) {
      return i+1;
    }
  }
  return 0;
}

byte PatternManager::saveLedPatternMetadata(PatternMetadata * pat) {
  byte insert = findInsertLocation(pat->len);
  if (insert == 0) {
      pat->address = 0x300;
  } else {
      //address is on the page after the previous pattern
      pat->address = ((this->patterns[insert-1].address + this->patterns[insert-1].len) & 0xffffff00) + 0x100;
  }

  for (int i=this->patternCount; i>insert; i--) {
    memcpy(&this->patterns[i],&this->patterns[i-1],sizeof(PatternMetadata));
  }
  this->patternCount ++;

  memcpy(&this->patterns[insert],pat,sizeof(PatternMetadata));
  this->flash->writeBytes(0x100+sizeof(PatternMetadata)*insert,(byte *)(&this->patterns[insert]),(this->patternCount-insert)*sizeof(PatternMetadata));

  return insert;
}

void PatternManager::saveLedPatternBody(int pattern, uint32_t patternStartPage, byte * payload, uint32_t len) {
  PatternMetadata * pat = &this->patterns[pattern];

  uint32_t writeLocation = pat->address + patternStartPage*0x100;
  this->flash->writeBytes(writeLocation,payload,len);
}


int PatternManager::getPatternCount() {
  return this->patternCount;
}

int PatternManager::getSelectedPattern() {
  return this->selectedPattern;
}

PatternManager::PatternMetadata * PatternManager::getActivePattern() {
  return &this->patterns[this->getSelectedPattern()];
}

bool PatternManager::loadNextFrame(byte * ledBuffer, int ledCount) {
  if (this->getSelectedPattern() == -1) { //no pattern selected, turn LEDs off
    if (this->lastFrameTime != 0) return false;
    this->lastFrameTime = millis();

    for (int i=0; i<ledCount; i++) {
      ledBuffer[3*i+1] = 0;
      ledBuffer[3*i+0] = 0;
      ledBuffer[3*i+2] = 0;
    }
  }

  PatternMetadata * active = this->getActivePattern();
  if (millis() - this->lastFrameTime < 1000  / active->fps) return false; //wait for a frame based on fps
  this->lastFrameTime = millis();
  uint32_t width = active->len / (active->frames * 3); //width in pixels of the pattern
  uint32_t startAddress = active->address + (width * 3 * this->currentFrame);
  
  byte buf[width*3];
  this->flash->readBytes(startAddress,(byte*)buf,width*3);
  
  for (int i=0; i<ledCount; i++) {
    ledBuffer[3*i+1] = buf[(3*(i % width))+0] >> 4; //we reverse the byte order, strip reads in GRB, stored in RGB
    ledBuffer[3*i+0] = buf[(3*(i % width))+1] >> 4;
    ledBuffer[3*i+2] = buf[(3*(i % width))+2] >> 4;
  }
  this->currentFrame += 1;
  if (this->currentFrame >= active->frames) {
    this->currentFrame = 0;
  }

  return true;
}

int PatternManager::serializePatterns(byte * buf, int len) {
  PatternManager::PatternMetadata * pat;
  int index = 0;
  char * wh = (char *)buf;
  for (int i=0; i<this->getPatternCount(); i++) {
    pat = &this->patterns[i];
    int n = sprintf(wh,"%d,%s,%d,%d,%d,%d,%d\n",i,pat->name,pat->address,pat->len,pat->frames,pat->flags,pat->fps);
    index += n;
    wh += n;
  }
  return index;
}


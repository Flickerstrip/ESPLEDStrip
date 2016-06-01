// vim:ts=2 sw=2:
#include "PatternManager.h"

PatternManager::PatternManager(M25PXFlashMemory * mem) {
  this->flash = mem;
  this->patternCount = 0;
  this->lastSavedPattern = -1;
  this->testPatternActive = false;
  this->transitionDuration = 0;
  this->lastFrame = 0;
  this->freezeFrameIndex = -1;
}

void PatternManager::loadPatterns() {
  EEPROM.begin(EEPROM_SIZE);

  //The pattern count is stored in the first byte of the PATTERNS section of eeprom
  this->patternCount = EEPROM.read(EEPROM_PATTERNS_START);
  if (this->patternCount == 0xff) this->patternCount = 0;

  /*
  Serial.println("loading patterns");
  uint8_t * ptr = EEPROM.getDataPtr();
  debugHex((char*)(ptr+EEPROM_PATTERNS_START),20);

  Serial.print("Pattern count: ");
  Serial.print(this->patternCount);
  Serial.println();
  */

  //This is a selection sort that will repeatedly choose the lowest addressed pattern
  PatternReference ref;
  for (int patternIndex=0; patternIndex<this->patternCount; patternIndex++) {
    int earliestPattern = -1;
    uint32_t earliestAddress = 0;
    for (int i=0; i<this->patternCount; i++) {
      //Read the address/len from EEPROM
      for (int l=0; l<sizeof(PatternReference); l++) {
        ((byte *)(&ref))[l] = EEPROM.read(EEPROM_PATTERNS_START+1+i*sizeof(PatternReference)+l);
        /*
        Serial.print("read byte: [");
        Serial.print(i);
        Serial.print(" ");
        Serial.print(l);
        Serial.print("] ");
        Serial.print(((byte *)(&ref))[l]);
        Serial.println();
        */
      }
      /*
      Serial.print("address: ");
      Serial.print(ref.address);
      Serial.println();
      Serial.print("len: ");
      Serial.print(ref.len);
      Serial.println();
      */
      
      //is this pattern earlier in memory than the current choice? use it instead
      //                           [--           select the previous pattern's address or zero  --]
      /*
      Serial.println("min address");
      Serial.println(this->patterns[patternIndex-1].address);
      Serial.print("patternIndex: ");
      Serial.println(patternIndex);
      Serial.print("ref.address: ");
      Serial.println(ref.address);
      Serial.print("earliestAddress: ");
      Serial.println(earliestAddress);
      Serial.print("c1: ");
      Serial.println(patternIndex == 0 || this->patterns[patternIndex-1].address < ref.address);
      Serial.print("c2: ");
      Serial.println(earliestPattern == -1 || ref.address < earliestAddress);
      */
      if ((patternIndex == 0 || this->patterns[patternIndex-1].address < ref.address) && (earliestPattern == -1 || ref.address < earliestAddress)) {
        /*
        Serial.print("found new smallest address: ");
        Serial.println(ref.address);
        */
        earliestAddress = ref.address; //update min choice
        earliestPattern = i;
      }
    }

    /*
    Serial.print("Chose pattern index: ");
    Serial.print(earliestPattern);
    Serial.print(" with address ");
    Serial.print(earliestAddress,HEX);
    Serial.println();
    */

    //Load the first page of the chosen pattern that contains the metadata
    this->flash->readBytes(earliestAddress,(byte *)(&this->patterns[patternIndex]),sizeof(PatternMetadata));
    /*
    Serial.print("loaded pattern into index: ");
    Serial.println(patternIndex);
    Serial.print("pattern fps: ");
    Serial.println(this->patterns[patternIndex].fps);
    */
  }
  EEPROM.end();

  //we should have a sorted list of patterns now.. lets check
  /*
  Serial.println("finished loadng patterns: ");
  this->echoPatternTable();
  */
}

void PatternManager::echoPatternTable() {
  for (int i=0; i<this->patternCount; i++) {
    Serial.print("[");
    Serial.print(i);
    Serial.print("] 0x");
    Serial.print(this->patterns[i].address,HEX);
    Serial.print(" len:");
    Serial.print(this->patterns[i].len,DEC);
    Serial.print(" '");
    Serial.print(this->patterns[i].name);
    Serial.print("'");
    Serial.println();
  }
}

void PatternManager::resetPatternsToDefault() {
  this->clearPatterns();

  PatternMetadata newpat;
  char patternName[] = "Default";
  memcpy(newpat.name, patternName, strlen(patternName)+1);
  newpat.len = 1*3*50;
  newpat.frames = 50;
  newpat.flags = 0;
  newpat.fps = 10;
  int led = 0;
  for (int i=0; i<10; i++) {
    this->buf[led*3+0] = i*25;
    this->buf[led*3+1] = 0;
    this->buf[led*3+2] = 0;
    led++;
  }
  for (int i=0; i<10; i++) {
    this->buf[led*3+0] = 250-i*25;
    this->buf[led*3+1] = i*25;
    this->buf[led*3+2] = 0;
    led++;
  }
  for (int i=0; i<10; i++) {
    this->buf[led*3+0] = 0;
    this->buf[led*3+1] = 250-i*25;
    this->buf[led*3+2] = i*25;
    led++;
  }
  for (int i=0; i<10; i++) {
    this->buf[led*3+0] = i*25;
    this->buf[led*3+1] = i*25;
    this->buf[led*3+2] = 250;
    led++;
  }
  for (int i=0; i<10; i++) {
    this->buf[led*3+0] = 250-i*25;
    this->buf[led*3+1] = 250-i*25;
    this->buf[led*3+2] = 250-i*25;
    led++;
  }

  byte patindex = this->saveLedPatternMetadata(&newpat);
  this->saveLedPatternBody(patindex,0,(byte*)this->buf,newpat.len);
}

void PatternManager::clearPatterns() {
  for (int i=0; i<this->patternCount; i++) {
    uint32_t first = this->patterns[i].address & 0xfffff000;
    uint32_t count = (this->patterns[i].len / 0x1000) + 1;
    for (int l=0; l<count; l++) {
      this->flash->eraseSubsector(first+l);
    }
  }

  EEPROM.begin(EEPROM_SIZE);
  for (int i=EEPROM_PATTERNS_START; i<EEPROM_SIZE; i++) {
    EEPROM.write(i,i == 0 ? 0 : 0xff);
  }
  EEPROM.end();

  this->patternCount = 0;
  this->selectedPattern = 0xff;
  lastSavedPattern = -1;
}

void PatternManager::selectPattern(byte n) {
    this->freezeFrameIndex = -1;

    if (this->patternCount == 0) {
      this->selectedPattern = -1;
      return;
    }

    if (n >= this->patternCount) return;

    //Store the transition information
    this->prev_selectedPattern = this->selectedPattern;
    this->patternTransitionTime = millis();
    prev = current;

    this->selectedPattern = n;

    PatternMetadata * pat = &this->patterns[n];

    /*
    Serial.println("selected pattern");
    Serial.print("name: ");
    Serial.print(pat->name);
    Serial.println();
    Serial.print("address: ");
    Serial.print(pat->address);
    Serial.println();
    Serial.print("len: ");
    Serial.print(pat->len);
    Serial.println();
    Serial.print("fps: ");
    Serial.print(pat->fps);
    Serial.println();
    */

    current = RunningPattern(pat,this->buf);
}

void PatternManager::setTransitionDuration(int duration) {
  this->transitionDuration = duration;
}

void PatternManager::deletePattern(byte n) {
  /*
  Serial.print("Deleting pattern! ");
  Serial.println(n);
  */
  uint32_t address = this->patterns[n].address;
  uint32_t first = this->patterns[n].address & 0xfffff000;
  uint32_t count = (this->patterns[n].len / 0x1000) + 1;
  /*
  Serial.print("first: ");
  Serial.print(first);
  Serial.println();
  Serial.print("count: ");
  Serial.print(count);
  Serial.println();
  */

  for (int l=0; l<count; l++) {
    //Serial.println("Erasing subsector: ");
    //Serial.println(first+l,HEX);
    this->flash->eraseSubsector(first+l);
  }

  EEPROM.begin(EEPROM_SIZE);

  //Serial.print("deleting pattern at address: ");
  //Serial.println(address);
  PatternReference ref;
  bool found = false;
  for (int i=0; i<this->patternCount; i++) {
      if (!found) {
        for (int l=0; l<sizeof(PatternReference); l++) {
          ((byte *)(&ref))[l] = EEPROM.read(EEPROM_PATTERNS_START+1+i*sizeof(PatternReference)+l);
        }
        //Serial.print("ref.address: ");
        //Serial.println(ref.address);
        if (address == ref.address) {
          //Serial.print("found address at index");
          //Serial.println(i);
          found = true;
        }
      }

      if (found) {
        //Serial.print("copying pattern into: ");
        //Serial.println(i);
        for (int l=0; l<sizeof(PatternReference); l++) {
          byte val = i==this->patternCount-1 ? 0xff : EEPROM.read(EEPROM_PATTERNS_START+1+(i+1)*sizeof(PatternReference)+l);
          EEPROM.write(EEPROM_PATTERNS_START+1+i*sizeof(PatternReference)+l,val);
        }
      }
  }
  this->patternCount--;
  EEPROM.write(EEPROM_PATTERNS_START,this->patternCount);
  EEPROM.end();

  for (int i=n; i<this->patternCount; i++) {
    memcpy(&this->patterns[i],&this->patterns[i+1],sizeof(PatternMetadata));
  }

  if (n <= this->selectedPattern) {
    selectPattern(this->selectedPattern-1);
  }

  if (this->patternCount == 0) this->selectedPattern = -1;
}

uint32_t PatternManager::findInsertLocation(uint32_t len) {
  for (int i=0; i<this->patternCount; i++) {
    uint32_t firstAvailableSubsector = ((this->patterns[i].address + this->patterns[i].len) & 0xfffff000) + 0x1000;

    if (i == this->patternCount - 1) return i+1; //last pattern

    uint32_t spaceAfter = this->patterns[i+1].address - firstAvailableSubsector;
    if (spaceAfter > len) {
      return i+1;
    }
  }
  return 0;
}

byte PatternManager::saveLedPatternMetadata(PatternMetadata * pat) {
  pat->len += 0x100; //we're adding a page for metadata storage

  byte insert = findInsertLocation(pat->len);
  //Serial.print("inserting pattern at: ");
  //Serial.print(insert);
  //Serial.println();
  if (insert == 0) {
      pat->address = 0;
  } else {
      //address is on the subsector after the previous pattern
      pat->address = ((this->patterns[insert-1].address + this->patterns[insert-1].len) & 0xfffff000) + 0x1000;
  }

  //Serial.println("PatternMetadata for write: ");
  //debugHex(((char*)pat),sizeof(PatternMetadata));

  for (int i=this->patternCount; i>insert; i--) {
    memcpy(&this->patterns[i],&this->patterns[i-1],sizeof(PatternMetadata));
  }

  //Write reference information to EEPROM
  EEPROM.begin(EEPROM_SIZE);
  PatternReference ref;
  ref.address = pat->address;
  ref.len = pat->len;
  //Serial.print("Address: ");
  //Serial.print(ref.address);
  //Serial.println();
  //Serial.print("len: ");
  //Serial.print(ref.len);
  //Serial.println();
  for (int i=0; i<sizeof(PatternReference); i++) {
    //Serial.print("Writing byte: ");
    //Serial.print(((byte *)(&ref))[i],HEX);
    //Serial.print(" to ");
    //Serial.print(EEPROM_PATTERNS_START+1+sizeof(PatternReference)*this->patternCount+i,DEC);
    //Serial.println();
    EEPROM.write(EEPROM_PATTERNS_START+1+sizeof(PatternReference)*this->patternCount+i,((byte *)(&ref))[i]);
  }

  this->patternCount++;
  //Serial.print("Writing byte: ");
  //Serial.print(this->patternCount);
  //Serial.print(" to ");
  //Serial.print(EEPROM_PATTERNS_START);
  //Serial.println();
  EEPROM.write(EEPROM_PATTERNS_START,this->patternCount);

  EEPROM.end();

  //Serial.println("finished writing pattern info");
  //EEPROM.begin(EEPROM_SIZE);
  //debugHex((char*)(EEPROM.getDataPtr()+EEPROM_PATTERNS_START),20);
  //EEPROM.end();

  memcpy(&this->patterns[insert],pat,sizeof(PatternMetadata));
  this->flash->programBytes(pat->address,(byte *)(&this->patterns[insert]),sizeof(PatternMetadata)); //write a single page with the metadata info

  return insert;
}

void PatternManager::saveLedPatternBody(int pattern, uint32_t patternStartPage, byte * payload, uint32_t len) {
  PatternMetadata * pat = &this->patterns[pattern];

  //                                   metadata page
  //                                        |
  uint32_t writeLocation = pat->address + 0x100 + patternStartPage*0x100;
  //Serial.print("pattern page: ");
  //Serial.println(patternStartPage);
  //Serial.print("len: ");
  //Serial.println(len);
  //Serial.print("address: ");
  //Serial.println(pat->address);
  //Serial.print("writing location: ");
  //Serial.println(writeLocation);
  this->flash->programBytes(writeLocation,payload,len);
}

void PatternManager::saveTestPattern(PatternMetadata * pat) {
  pat->len += 0x100; //we're adding a page for metadata storage

  byte insert = findInsertLocation(pat->len);
  
  if (insert == 0) {
      pat->address = 0;
  } else {
      //address is on the subsector after the previous pattern
      pat->address = ((this->patterns[insert-1].address + this->patterns[insert-1].len) & 0xfffff000) + 0x1000;
  }

  //Serial.println("saving test pattern!");
  EEPROM.begin(EEPROM_SIZE);
  PatternReference ref;

  bool hasTestPattern = false;
  for (int l=0; l<sizeof(PatternReference); l++) {
    ((byte *)(&ref))[l] = EEPROM.read(EEPROM_TEST_PATTERN+l);

    if (EEPROM.read(EEPROM_TEST_PATTERN+l) != 0xff) hasTestPattern = true;
  }
  //Serial.println("existing test pattern");
  //Serial.print("ref.address: ");
  //Serial.println(ref.address);
  //Serial.print("ref.len: ");
  //Serial.println(ref.len);

  //clear out existing test pattern
  if (hasTestPattern) {
      Serial.println("erasing test pattern");
      uint32_t first = ref.address & 0xfffff000;
      uint32_t count = (ref.len / 0x1000) + 1;
      //Serial.print("first: ");
      //Serial.println(first);
      //Serial.print("count: ");
      //Serial.println(count);
      for (int l=0; l<count; l++) {
        //Serial.println("Erasing subsector: ");
        //Serial.println(first+l,HEX);
        this->flash->eraseSubsector(first+l);
      }
  }


  //TODO dedup this.. lets integrate the test pattern into the set of normal patterns..
  ref.address = pat->address;
  ref.len = pat->len;
  //Serial.print("Address: ");
  //Serial.print(ref.address);
  //Serial.println();
  //Serial.print("len: ");
  //Serial.print(ref.len);
  //Serial.println();
  for (int i=0; i<sizeof(PatternReference); i++) {
    //Serial.print("Writing byte: ");
    //Serial.print(((byte *)(&ref))[i],HEX);
    //Serial.print(" to ");
    //Serial.print(EEPROM_TEST_PATTERN+i,DEC);
    //Serial.println();
    EEPROM.write(EEPROM_TEST_PATTERN+i,((byte *)(&ref))[i]);
  }
  EEPROM.end();

  memcpy(&this->testPattern,pat,sizeof(PatternMetadata));
  //Serial.println("finished saving test pattern");
}

void PatternManager::saveTestPatternBody(uint32_t patternStartPage, byte * payload, uint32_t len) {
  //Serial.print("saving test body: ");
  //Serial.println(patternStartPage);

  uint32_t writeLocation = testPattern.address + 0x100 + patternStartPage*0x100;
  this->flash->programBytes(writeLocation,payload,len);
}

void PatternManager::showTestPattern(bool show) {
  this->testPatternActive = show;
  this->freezeFrameIndex = -1;

  if (show) {
    this->prev_selectedPattern = 0;
    this->selectedPattern = 0;
    this->patternTransitionTime = millis() - this->transitionDuration;;

    PatternMetadata * pat = &this->testPattern;
    //Serial.println("showing test pattern");
    //Serial.print("fps: ");
    //Serial.println(pat->fps);
    current = RunningPattern(pat,this->buf);
  }
}

int PatternManager::getTotalBlocks() {
  return PatternManager::NUM_SUBSECTORS;
}

int PatternManager::getUsedBlocks() {
  int blocksUsed = 0;
  for (int i=0; i<this->patternCount;i++) {
    uint32_t usedPages = this->patterns[i].len / 0x100;
    if (this->patterns[i].len % 0x100 != 0) usedPages++;

    blocksUsed += usedPages;
  }

  return blocksUsed;
}

int PatternManager::getAvailableBlocks() {
  return getTotalBlocks() - getUsedBlocks();
}

int PatternManager::getPatternCount() {
  return this->patternCount;
}

int PatternManager::getSelectedPattern() {
  return this->selectedPattern;
}

int PatternManager::getCurrentFrame() {
  return this->current.getCurrentFrame();
}

int PatternManager::getPatternIndexByName(const char * name) {
  for (int i=0; i<this->patternCount; i++) {
    if (strcmp(this->patterns[i].name,name) == 0) {
      return i;
    }
  }
  return -1;
}

bool PatternManager::isTestPatternActive() {
  return this->testPatternActive;
}

PatternMetadata * PatternManager::getActivePattern() {
  if (this->testPatternActive) return &this->testPattern;
  return &this->patterns[this->getSelectedPattern()];
}

PatternMetadata * PatternManager::getPrevPattern() {
  if (this->testPatternActive) return &this->testPattern;
  return &this->patterns[this->prev_selectedPattern];
}

void PatternManager::syncToFrame(int frame, int pingDelay) {
  this->current.syncToFrame(frame,pingDelay);
}

void PatternManager::freezeFrame(int frame) {
  if (frame == -1) frame = current.getCurrentFrame(); //pass -1 to freeze at current frame

  if (frame < 0) frame = 0;
  if (frame >= this->getActivePattern()->frames) frame = this->getActivePattern()->frames-1;

  this->freezeFrameIndex = frame;
}

bool PatternManager::loadNextFrame(LEDStrip * strip) {
  if (this->patternCount == 0) {
    strip->clear();
    return true;
  }

  bool isTransitioning = millis() - this->patternTransitionTime < this->transitionDuration;//we're in the middle of a transition

  bool needsUpdate = false; 

  if (this->freezeFrameIndex >= 0) {
    //we're in freeze frame mode
    needsUpdate = (millis() - this->lastFrame) > 1000/5; //update at 5fps if we're running a single frame
  } else {
    needsUpdate |= current.needsUpdate();
    if (isTransitioning) needsUpdate |= prev.needsUpdate();
  }

  if (!needsUpdate && millis() - this->lastFrame < 30) {
    return false;
  }

  if (this->freezeFrameIndex >= 0) {
    current.loadFrame(strip,this->flash, 1, this->freezeFrameIndex);
  } else if (isTransitioning) {
    strip->clear();
    float transitionFactor =  (millis() - this->patternTransitionTime) / (float)this->transitionDuration;
    current.loadNextFrame(strip,this->flash, transitionFactor);
    prev.loadNextFrame(strip,this->flash,1.0 - transitionFactor);
  } else {
    current.loadNextFrame(strip,this->flash, 1);
  }

  this->lastFrame = millis();
  return true;
}

/*
{
  'patterns':[
    {
      'index':index,
      'name':'Strip name',
      'address':addy,
      'length':len,
      'frame':frames,
      'flags':flags,
      'fps':fps
    }
  ],
  'selectedPattern':5,
  'brightness':50,
  'memory':{
    'used':100,
    'free':100,
    'total':100
  }
}
*/
int PatternManager::serializePatterns(char * buf, int bufferSize) {
  PatternMetadata * pat;
  char * ptr = buf;
  int size;

  size = snprintf(ptr,bufferSize,"[");
  ptr += size;
  bufferSize -= size;

  for (int i=0; i<this->getPatternCount(); i++) {
    //Serial.print("pattern loop: ");
    //Serial.print(i);
    //Serial.print("size: ");
    //Serial.println(bufferSize);
    if (i != 0) {
      size = snprintf(ptr,bufferSize,",");
      ptr += size;
      bufferSize -= size;
    }
    pat = &this->patterns[i];

    size = snprintf(ptr,bufferSize,"{\"index\":%d,\"name\":\"%s\",\"address\":%d,\"length\":%d,\"frames\":%d,\"flags\":%d,\"fps\":%d}",i,pat->name,pat->address,pat->len,pat->frames,pat->flags,pat->fps);
    ptr += size;
    bufferSize -= size;
  }

  size = snprintf(ptr,bufferSize,"]");
  ptr += size;
  bufferSize -= size;

  return ptr - buf;
}


void PatternManager::jsonPatterns(JsonArray& arr) {
  for (int i=0; i<this->getPatternCount(); i++) {
    JsonObject& json = arr.createNestedObject();
    json["index"] = i;
    json["name"] = this->patterns[i].name;
    json["address"] = this->patterns[i].address;
    json["length"] = this->patterns[i].len;
    json["frames"] = this->patterns[i].frames;
    json["flags"] = this->patterns[i].flags;
    json["fps"] = this->patterns[i].fps;
  }
}

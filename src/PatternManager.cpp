// vim:ts=4 sw=4:
#include "PatternManager.h"

PatternManager::PatternManager(M25PXFlashMemory * mem) {
    this->flash = mem;
    this->patternCount = 0;
    this->lastSavedPattern = -1;
    this->transitionDuration = 0;
    this->lastFrame = 0;
    this->freezeFrameIndex = -1;
}

void PatternManager::loadPatterns() {
    //The pattern count is stored in the first byte of the PATTERNS section of eeprom
    EEPROM.begin(EEPROM_SIZE); //IMPORTANT: Use EEPROM_SIZE or EEPROM will be cleared down to this parameter
    int patternsToLoad = EEPROM.read(EEPROM_PATTERNS_START);
    EEPROM.end();

    if (patternsToLoad == 0xff) patternsToLoad = 1;

    this->patterns[0].address = 0xffffffff; //initialize the preview pattern correctly

    this->patternCount = 0;
    //NOTE: Test pattern inhabits patterns[0] and is counted in the patternCount
    for (int patternIndex=0; patternIndex<patternsToLoad; patternIndex++) {
        PatternReference ref;
        EEPROM.begin(EEPROM_SIZE); //IMPORTANT: Use EEPROM_SIZE or EEPROM will be cleared down to this parameter
        memcpy(&ref,EEPROM.getDataPtr()+EEPROM_PATTERNS_START+1+patternIndex*sizeof(PatternReference),sizeof(PatternReference));
        EEPROM.end();

        //insert into the sorted array
        this->insertPatternReference(&ref);

        if (ref.address != 0xffffffff) { //This indicates an empty/invalid pattern (used primarily for the test pattern)
            //Load the first page of the chosen pattern that contains the metadata
            this->flash->readBytes(ref.address,(byte *)(&this->patterns[patternIndex]),sizeof(PatternMetadata));
        }

        this->patternCount++;
    }
}

int PatternManager::insertPatternReference(PatternReference * ref) { //Does not increment pattern count
    int insertLocation;
    uint8_t addressedPatterns = this->hasTestPattern() ? this->patternCount-1 : this->patternCount;
    for (insertLocation=0; insertLocation<addressedPatterns; insertLocation++) {
        if (ref->address < this->patternsByAddress[insertLocation].address) {
            for (int i=this->patternCount; i>insertLocation; i--) {
                memcpy(&this->patternsByAddress[i],&this->patternsByAddress[i-1],sizeof(PatternReference));
            }
            break;
        }
    }
    memcpy(&this->patternsByAddress[insertLocation],ref,sizeof(PatternReference));

    return insertLocation;
}

void PatternManager::saveReferenceTable() {
    EEPROM.begin(EEPROM_SIZE); //IMPORTANT: Use EEPROM_SIZE or EEPROM will be cleared down to this parameter
    EEPROM.write(EEPROM_PATTERNS_START,this->patternCount);

    uint8_t * ptr = EEPROM.getDataPtr();
    for (int i=0; i<this->patternCount; i++) {
        PatternReference ref;
        ref.id = patterns[i].id;
        ref.address = patterns[i].address;
        ref.len = patterns[i].len;
        memcpy(ptr+EEPROM_PATTERNS_START+1+i*sizeof(PatternReference),&ref,sizeof(PatternReference));
    }

    for (int i=(this->patternCount)*sizeof(PatternReference); i<MAX_PATTERNS*sizeof(PatternReference); i++) {
        EEPROM.write(EEPROM_PATTERNS_START+1+i,0xff); //clear out the remaining spaces
    }

    EEPROM.end();
}

uint8_t PatternManager::createId() {
    uint8_t used[this->patternCount];
    uint8_t pats = 0;
    for (uint8_t i=0; i<this->patternCount; i++) {
        if (this->patterns[i].address == 0xffffffff) continue;
        used[pats] = this->patterns[i].id;
        pats ++;
    }

    //Selection sort the array of IDs
    for (uint8_t i=0; i<pats; i++) {
        uint8_t minIndex = i;
        for (uint8_t l=i; l<pats; l++) {
            if (used[l] < used[minIndex]) minIndex = l;
        }

        uint8_t tmp = used[i];
        used[i] = used[minIndex];
        used[minIndex] = tmp;
        if (i != 0 && used[i-1]+1 < used[i]) { //short circuit the sort if we find that theres a > 1 difference between this item and the previous one
            return used[i-1]+1;
        }
    }

    if (pats == 0 || used[0] != 1) {
        return 1;
    }

    return pats + 1;
}

//DEBUG
void PatternManager::echoPatternTable() {
    for (int i=0; i<this->patternCount; i++) {
        Serial.print("[");
        Serial.print(i);
        if (this->patterns[i].address == 0xffffffff) {
            Serial.println("] EMPTY");
            continue;
        }
        Serial.print("] (id: ");
        Serial.print(this->patterns[i].id);
        Serial.print(") 0x");
        Serial.print(this->patterns[i].address,HEX);
        Serial.print(" len:");
        Serial.print(this->patterns[i].len,DEC);
        Serial.print(" name:");
        Serial.print(this->patterns[i].name);
        Serial.println();
    }

    Serial.println("Patterns by address: ");
    uint8_t addressedPatterns = this->hasTestPattern() ? this->patternCount-1 : this->patternCount;
    for (int i=0; i<addressedPatterns; i++) {
        Serial.print("[");
        Serial.print(i);
        Serial.print("] (id ");
        Serial.print(this->patternsByAddress[i].id);
        Serial.print(") 0x");
        Serial.print(this->patternsByAddress[i].address,HEX);
        Serial.print(" len:");
        Serial.print(this->patternsByAddress[i].len,DEC);
        Serial.println();
    }
}

//DEBUG
void PatternManager::checkPatterns() {
    Serial.println("Patterns by address: ");
    uint8_t addressedPatterns = this->hasTestPattern() ? this->patternCount-1 : this->patternCount;
    for (int i=0; i<addressedPatterns; i++) {
        PatternReference * pat = &this->patternsByAddress[i];

        int brokenPages = this->checkPattern(pat);

        Serial.print("[");
        Serial.print(i);
        Serial.print("] (id ");
        Serial.print(pat->id);
        Serial.print(") 0x");
        Serial.print(pat->address,HEX);
        Serial.print(" len:");
        Serial.print(pat->len,DEC);
        Serial.print(" (");
        Serial.print(brokenPages);
        Serial.print(")");
        Serial.println();
    }
}

//DEBUG
int PatternManager::checkPattern(PatternReference * pat) {
    int pageLength = 0x100;
    byte checkbuf[pageLength];

    uint32_t readLocation = pat->address + 0x100;
    uint32_t bytesRead = 0;
    int brokenPages = 0;
    while(bytesRead < pat->len - 0x100) {
        yield();
        int bytesToRead = min(pageLength,(int)(pat->len - 0x100 - bytesRead));
        this->flash->readBytes(readLocation + bytesRead,(byte*)&checkbuf,bytesToRead);
        bytesRead += bytesToRead;

        bool foundNonwhite = false;
        for(int i=0; i<bytesToRead; i++) {
            if (checkbuf[i] != 0xff) {
                foundNonwhite = true;
                break;
            }
        }
        if (!foundNonwhite) {
            brokenPages++;
        }
    }

    return brokenPages;
}

void PatternManager::checkPatternById(uint8_t id) {
    PatternReference * pat = this->findPatternReferenceById(id);
    int brokenPages = this->checkPattern(pat);
    if (brokenPages > 0) {
        Serial.print("Pattern has ");
        Serial.print(brokenPages);
        Serial.print(" broken pages (id ");
        Serial.print(id);
        Serial.println();
    }
}

void PatternManager::addPatternFromProgmem(const PatternDefinition * patternDef) {
    PatternMetadata newpat;
    memcpy(newpat.name, patternDef->name, strlen(patternDef->name)+1);
    newpat.frames = patternDef->frames;
    newpat.pixels = patternDef->pixels;
    newpat.fps = patternDef->fps;
    newpat.flags = 0;

    uint8_t id = this->saveLedPatternMetadata(&newpat,false);

    int remaining = patternDef->size;
    int page = 0;
    while(remaining > 0) {
        int pageReadSize = 0x100;
        if (remaining < pageReadSize) pageReadSize = remaining;
        for (int i=0; i<pageReadSize; i++) {
            byte a = pgm_read_byte_near(patternDef->data+i+(page*0x100));
            this->buf[i] = a;
        }
        remaining -= pageReadSize;
        this->saveLedPatternBody(id,page++,(byte*)&buf,0x100);
    }
}

void PatternManager::resetPatternsToDefault() {
    this->clearPatterns();

    this->addPatternFromProgmem(&patternDef154); //Adds Carnival Rainbow
    this->addPatternFromProgmem(&patternDef45); //Adds Chasing Rainbow 15
    this->addPatternFromProgmem(&patternDef173); //Adds Cracksauce
    this->addPatternFromProgmem(&patternDef2); //Adds Ember Crawl
    this->addPatternFromProgmem(&patternDef25); //Adds Fire Sparkle
    this->addPatternFromProgmem(&patternDef541); //Adds LED Totem Color Splash
    this->addPatternFromProgmem(&patternDef539); //Adds LED Totem Rainbow Spread
    this->addPatternFromProgmem(&patternDef540); //Adds LED Totem Spiral Bounce
    this->addPatternFromProgmem(&patternDef367); //Adds PanShift
    this->addPatternFromProgmem(&patternDef672); //Adds Police Lights - Quinte Flash
    this->addPatternFromProgmem(&patternDef546); //Adds Rainbow Chase
    this->addPatternFromProgmem(&patternDef642); //Adds Sam- JulieSetting2
    this->addPatternFromProgmem(&patternDef249); //Adds Some Fireworks
}

void PatternManager::clearPatterns() {
    for (int i=0; i<this->patternCount; i++) {
        if (this->patterns[i].address == 0xffffffff) continue; //don't delete invalid/disabled patterns

        uint32_t first = this->patterns[i].address & 0xfffff000;
        uint32_t count = (this->patterns[i].len / 0x1000) + 1;
        for (int l=0; l<count; l++) {
            uint32_t addr = first+(0x1000 * l);
            Serial.print("Erasing subsector: 0x");
            Serial.println(addr,HEX);
            this->flash->eraseSubsector(addr);
        }
    }

    for (int i=0; i<sizeof(PatternMetadata); i++) ((byte *)this->patterns)[i] = 0xff;
    this->patternCount = 1;

    this->saveReferenceTable();

    this->selectedPattern = 0xff;
    lastSavedPattern = -1;
}

void PatternManager::selectPatternByIndex(byte n) {
    n = n + 1; //0xff is used for test pattern

    this->freezeFrameIndex = -1;

    if (this->patterns[n].address == 0xffffffff) return;
    if (n >= this->patternCount) n = this->patternCount - 1;

    //Store the transition information
    this->prev_selectedPattern = this->selectedPattern;
    this->patternTransitionTime = millis();
    prev = current;

    this->selectedPattern = n;

    PatternMetadata * pat = &this->patterns[n];

    current = RunningPattern(pat,this->buf);
}

void PatternManager::selectPatternById(uint8_t patternId) {
    this->selectPatternByIndex(this->findPatternById(patternId) - 1); //deals with the offset
}

bool PatternManager::isValidPatternId(uint8_t patternId) {
    return this->findPatternById(patternId) != 0xff;
}

void PatternManager::setTransitionDuration(int duration) {
    this->transitionDuration = duration;
}

void PatternManager::deletePatternByIndex(byte n) {
    n = n + 1;
    uint32_t address = this->patterns[n].address;
    uint32_t first = this->patterns[n].address & 0xfffff000;
    uint32_t count = (this->patterns[n].len / 0x1000) + 1;

    if (address != 0xffffffff) {
        for (int l=0; l<count; l++) {
            uint32_t addr = first+(0x1000 * l);
            Serial.print("Erasing subsector: 0x");
            Serial.print(addr,HEX);
            this->flash->eraseSubsector(addr);
        }

        uint8_t addressedPatterns = this->hasTestPattern() ? this->patternCount-1 : this->patternCount;
        for (int i=0; i<addressedPatterns; i++) {
            if (this->patternsByAddress[i].address == address) {
                for (int l=i; l<this->patternCount; l++) {
                    memcpy(&this->patternsByAddress[l],&this->patternsByAddress[l+1],sizeof(PatternReference));
                }
            }
        }
        if (n != 0) this->patternCount--;

        this->saveReferenceTable();
    }

    if (n == 0) {
        //Clear out the test pattern without shifting anything, this allows us to delete the test pattern without ruining our placement
        for (int i=0; i<sizeof(PatternMetadata); i++) {
            ((byte*)&this->patterns[0])[i] = 0xff;
        }
    } else {
        //Delete from patterns
        for (int i=n; i<this->patternCount; i++) {
            memcpy(&this->patterns[i],&this->patterns[i+1],sizeof(PatternMetadata));
        }

        if (n <= this->selectedPattern) {
            selectPatternByIndex(this->selectedPattern-1);
        }

        if (this->patternCount == 0) this->selectedPattern = -1;
    }
}

void PatternManager::deletePatternById(uint8_t patternId) {
    this->deletePatternByIndex(this->findPatternById(patternId) - 1); //deals with the offset
}

bool PatternManager::hasTestPattern() {
    return this->patternCount != 0 && this->patterns[0].address == 0xffffffff;
}

int PatternManager::findInsertLocation(uint32_t len) {
    uint8_t pats = 0;
    uint8_t addressedPatterns = this->hasTestPattern() ? this->patternCount-1 : this->patternCount;
    for (int i=0; i<addressedPatterns; i++) {
        uint32_t firstAvailableSubsector = ((this->patternsByAddress[i].address + this->patternsByAddress[i].len) & 0xfffff000) + 0x1000;

        if (i == this->patternCount - 1) {
            uint32_t lastAddressableByte = (NUM_SUBSECTORS * 0x1000) - 1;
            if (firstAvailableSubsector + len > lastAddressableByte) return -1; //we're out of space!

            return i+1; //last pattern
        }

        uint32_t spaceAfter = this->patternsByAddress[i+1].address - firstAvailableSubsector;
        if (spaceAfter > len) {
            return i+1;
        }
    }
    return 0;
}

int PatternManager::saveLedPatternMetadata(PatternMetadata * pat, bool previewPattern) {
    pat->len = pat->pixels * pat->frames * 3 + 0x100; //calculate the memory footprint

    if (previewPattern) this->deletePatternByIndex(-1);

    int insert = findInsertLocation(pat->len); //find the memory insert location
    if (insert == -1) return -1;
    if (insert == 0) {
        pat->address = 0;
    } else {
        //address is on the subsector after the previous pattern
        pat->address = ((this->patternsByAddress[insert-1].address + this->patternsByAddress[insert-1].len) & 0xfffff000) + 0x1000;
    }

    pat->id = this->createId();

    PatternReference ref;
    ref.id = pat->id;
    ref.address = pat->address;
    ref.len = pat->len;

    byte insertloc = this->insertPatternReference(&ref);

    byte patternIndex = this->patternCount;
    if (previewPattern) patternIndex = 0;

    memcpy(&this->patterns[patternIndex],pat,sizeof(PatternMetadata));
    this->flash->programBytes(pat->address,(byte *)pat,sizeof(PatternMetadata)); //write a single page with the metadata info

    if (!previewPattern) this->patternCount++;
    this->saveReferenceTable();

    return pat->id;
}

PatternReference * PatternManager::findPatternReferenceById(uint8_t patternId) {
    uint8_t addressedPatterns = this->hasTestPattern() ? this->patternCount-1 : this->patternCount;
    for (int i=0; i<addressedPatterns; i++) {
        PatternReference * ref = &this->patternsByAddress[i];
        if (ref->id == patternId) return ref;
    }
    return NULL;
}

uint8_t PatternManager::findPatternById(uint8_t patternId) {
    for (int i=0; i<this->patternCount; i++) {
        if (this->patterns[i].id == patternId) return i;
    }
    return 0xff;
}

void PatternManager::saveLedPatternBody(uint8_t patternId, uint32_t patternStartPage, byte * payload, uint32_t len) {
    uint8_t patternIndex = this->findPatternById(patternId);
    if (patternIndex == 0xff) return;

    PatternMetadata * pat = &this->patterns[patternIndex];

    uint32_t writeLocation = pat->address + 0x100 + patternStartPage*0x100;

#ifdef DEBUG_SAVE
    byte checkbuf[len];
    this->flash->readBytes(writeLocation,(byte*)&checkbuf,len);
    bool clean = true;
    for (int i=0; i<len; i++) {
        if (checkbuf[i] != 0xff) {
            clean = false;
            break;
        }
    }
    if (clean == false) {
        Serial.print("Writing pattern ");
        Serial.print(patternId);
        Serial.print(" [0x");
        Serial.print(writeLocation,HEX);
        Serial.print("]");
        Serial.print(" UNCLEAN!");
        Serial.println();
    }
#endif

    this->flash->programBytes(writeLocation,payload,len);
}

int PatternManager::getTotalBlocks() {
    return PatternManager::NUM_SUBSECTORS;
}

int PatternManager::getUsedBlocks() {
    int blocksUsed = 0;
    for (int i=0; i<this->patternCount;i++) {
        if (this->patterns[i].address == 0xffffffff) continue;

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
    return this->patternCount-1;
}

byte PatternManager::getSelectedIndex() {
    return this->selectedPattern-1;
}

byte PatternManager::getSelectedId() {
    return this->getActivePattern()->id;
}

int PatternManager::getCurrentFrame() {
    return this->current.getCurrentFrame();
}

int PatternManager::getPatternIndexByName(const char * name) {
    for (int i=0; i<this->patternCount; i++) {
        if (this->patterns[i].address == 0xffffffff) continue;
        if (strcmp(this->patterns[i].name,name) == 0) {
            return i-1;
        }
    }
    return -1;
}

bool PatternManager::isTestPatternActive() {
    return this->selectedPattern == 0;
}

PatternMetadata * PatternManager::getActivePattern() {
    return &this->patterns[this->selectedPattern];
}

PatternMetadata * PatternManager::getPrevPattern() {
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
    if (!current.hasPattern()) {
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
        float transitionFactor = (millis() - this->patternTransitionTime) / (float)this->transitionDuration;
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

    bool first = true;
    for (int i=1; i<this->patternCount; i++) {
        if (!first) {
            size = snprintf(ptr,bufferSize,",");
            ptr += size;
            bufferSize -= size;
        }
        pat = &this->patterns[i];

        size = snprintf(ptr,bufferSize,"{\"id\":%d,\"name\":\"%s\",\"frames\":%d,\"pixels\":%d,\"flags\":%d,\"fps\":%d}",pat->id,pat->name,pat->frames,pat->pixels,pat->flags,pat->fps);
        ptr += size;
        bufferSize -= size;
        first = false;
    }

    size = snprintf(ptr,bufferSize,"]");
    ptr += size;
    bufferSize -= size;

    return ptr - buf;
}

// Pass "countOnly" to prevent sending any data to the stream
int PatternManager::streamSerializePatterns(Stream * stream, bool countOnly) {
    PatternMetadata * pat;
    int written = 0;
    int size;
    long t;
    const int BUF_SIZE=150;
    char lineBuffer[BUF_SIZE];
    char * buf = (char*)&lineBuffer;

    size = snprintf(buf,BUF_SIZE,"[");
    written += size;
    if (!countOnly) stream->write(buf,size);

    bool first = true;
    for (int i=1; i<this->patternCount; i++) {
        if (!first) {
            size = snprintf(buf,BUF_SIZE,",");
            written += size;
            t = millis();
            if (!countOnly) stream->write(buf,size);
        }
        pat = &this->patterns[i];

        t = millis();
        size = snprintf(buf,BUF_SIZE,"{\"id\":%d,\"name\":\"%s\",\"frames\":%d,\"pixels\":%d,\"flags\":%d,\"fps\":%d}",pat->id,pat->name,pat->frames,pat->pixels,pat->flags,pat->fps);
        written += size;
        t = millis();
        if (!countOnly) stream->write(buf,size);
        first = false;
    }

    size = snprintf(buf,BUF_SIZE,"]");
    written += size;
    if (!countOnly) stream->write(buf,size);

    return written;
}


void PatternManager::jsonPatterns(JsonArray& arr) {
    for (int i=1; i<this->patternCount; i++) {
        JsonObject& json = arr.createNestedObject();
        json["id"] = this->patterns[i].id;
        json["name"] = this->patterns[i].name;
        json["frames"] = this->patterns[i].frames;
        json["pixels"] = this->patterns[i].pixels;
        json["flags"] = this->patterns[i].flags;
        json["fps"] = this->patterns[i].fps;
    }
}

int PatternManager::getPatternDataLength(uint8_t patternId) {
    uint8_t patternIndex = this->findPatternById(patternId);
    if (patternIndex == 0xff) return -1;

    PatternMetadata * pat = &this->patterns[patternIndex];
    return pat->len-0x100;
}

void PatternManager::writePatternData(uint8_t patternId, Stream * stream) {
    uint8_t patternIndex = this->findPatternById(patternId);
    if (patternIndex == 0xff) return;

    PatternMetadata * pat = &this->patterns[patternIndex];

    byte readbuf[0x100];
    for (uint32_t readOffset=0; readOffset<pat->len-0x100; readOffset += 0x100) {
        //read page
        uint32_t readLocation = pat->address + 0x100 + readOffset;
        uint32_t readLength = min(0x100,(int)(pat->len - 0x100 - readOffset));
        //Serial.print("reading at 0x");
        //Serial.print(readLocation,HEX);
        //Serial.print(" [");
        //Serial.print(readLength);
        //Serial.print("]");
        //Serial.println();
        this->flash->readBytes(readLocation,(byte*)&readbuf,readLength);
        stream->write(readbuf,readLength);
        //Serial.println("wrote page");
    }
}

// vim:ts=4 sw=4:
#include "PatternManager.h"

RunningPattern::RunningPattern() {
    this->pattern = NULL;
    this->currentFrame = 0;
    this->lastFrameTime = 0;
    this->buf = NULL;
}

RunningPattern::RunningPattern(PatternMetadata * pattern, char * buf) {
    this->pattern = pattern;
    this->currentFrame = 0;
    this->lastFrameTime = 0;
    this->buf = buf;
}

void RunningPattern::syncToFrame(int frame, int pingDelay) {
  if (this->pattern == NULL) return;

  this->currentFrame = frame;
  this->lastFrameTime = 0;
  long msPerFrame = 1000/this->pattern->fps;
  int rewindFrames = pingDelay / msPerFrame;
  int remaining = pingDelay - msPerFrame * rewindFrames;

  int nextFrameAt = millis() + remaining;
  this->lastFrameTime = nextFrameAt - msPerFrame;
}

bool RunningPattern::hasPattern() {
    return this->pattern != NULL;
}

bool RunningPattern::needsUpdate() {
  if (this->pattern == NULL) return true;
  /*
  Serial.print("last frame: ");
  Serial.println(this->lastFrameTime);
  Serial.print("fps: ");
  Serial.println(this->pattern->fps);
  Serial.print("millis: ");
  Serial.println(millis());
  */
  return (millis() - this->lastFrameTime) > (1000  / this->pattern->fps);
}

void RunningPattern::loadFrame(LEDStrip * strip, M25PXFlashMemory * flash, float multiplier, int frame) {
  if (this->pattern == NULL) return;

  uint32_t startAddress = this->pattern->address + 0x100 + (this->pattern->pixels * 3 * frame);

  byte * bbuf = (byte*)this->buf;
  flash->readBytes(startAddress,bbuf,this->pattern->pixels*3);

  if (globalDebug) {
      Serial.print("Loading frame ");
      Serial.print(frame);
      Serial.print(" (pat addr: 0x");
      Serial.print(this->pattern->address,HEX);
      Serial.print(") 0x");
      Serial.print(startAddress,HEX);
      Serial.print(" [0x");
      Serial.print(this->pattern->pixels*3,HEX);
      Serial.print(" bytes]");
      Serial.println();
      debugHex((char*)bbuf,this->pattern->pixels*3);
  }
  
  for (int i=0; i<strip->getLength(); i++) {
    if (multiplier != 1) {
        strip->addPixel(i,
            (byte)(((float)bbuf[(3*(i % this->pattern->pixels))+0])*multiplier),
            (byte)(((float)bbuf[(3*(i % this->pattern->pixels))+1])*multiplier),
            (byte)(((float)bbuf[(3*(i % this->pattern->pixels))+2])*multiplier) );
    } else {
        strip->setPixel(i,
            bbuf[(3*(i % this->pattern->pixels))+0],
            bbuf[(3*(i % this->pattern->pixels))+1],
            bbuf[(3*(i % this->pattern->pixels))+2] );
    }
  }
}

void RunningPattern::loadNextFrame(LEDStrip * strip, M25PXFlashMemory * flash, float multiplier) {
  if (this->pattern == NULL) return;

  //Serial.print("loading frame: ");
  //Serial.println(this->currentFrame);

  this->loadFrame(strip,flash,multiplier,this->currentFrame);


  if (this->needsUpdate()) {
    this->lastFrameTime = millis();

    this->currentFrame += 1;
    if (this->currentFrame >= this->pattern->frames) {
      this->currentFrame = 0;
    }
  }

  return;
}

int RunningPattern::getCurrentFrame() {
  return this->currentFrame;
}

RunningPattern& RunningPattern::operator=(const RunningPattern& a) {
    this->pattern = a.pattern;
    this->currentFrame = a.currentFrame;
    this->lastFrameTime = a.lastFrameTime;
    this->buf = a.buf;
}

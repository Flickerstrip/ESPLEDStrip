PROJECT_PATH:=$(shell dirname $(realpath $(lastword $(MAKEFILE_LIST))))
LAST_RELEASE=`git describe --abbrev=0`

all: build

build:
	mkdir -p $(PROJECT_PATH)/build
	/Applications/Arduino.app/Contents/MacOS/Arduino --pref build.path=$(PROJECT_PATH)/build --verify $(PROJECT_PATH)/ESPLED.ino
	cp $(PROJECT_PATH)/build/ESPLED.cpp.bin $(PROJECT_PATH)/releases/$(LAST_RELEASE).bin

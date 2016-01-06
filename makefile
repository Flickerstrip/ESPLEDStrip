PROJECT_PATH:=$(shell dirname $(realpath $(lastword $(MAKEFILE_LIST))))
LAST_RELEASE=`git describe --abbrev=0`

INO_FILES := $(wildcard *.ino)
CPP_FILES := $(wildcard *.cpp)
H_FILES := $(wildcard *.h)
SOURCE_FILES := $(INO_FILES) $(CPP_FILES) $(H_FILES)

print-%  : ; @echo $* = $($*)

all: build

build: $(SOURCE_FILES)
	mkdir -p $(PROJECT_PATH)/build
	/Applications/Arduino.app/Contents/MacOS/Arduino --pref build.path=$(PROJECT_PATH)/build --verify $(PROJECT_PATH)/FlickerstripFirmware.ino
	cp $(PROJECT_PATH)/build/FlickerstripFirmware.cpp.bin $(PROJECT_PATH)/releases/$(LAST_RELEASE).bin

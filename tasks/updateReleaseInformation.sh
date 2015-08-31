#!/bin/bash

echo "const char GIT_CURRENT_VERSION[] = \"`git describe`\";" > version.h

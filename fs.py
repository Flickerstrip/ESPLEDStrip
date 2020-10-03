#!/usr/bin/env python

import serial
import sys
import time
import uuid;
import csv
import os
import re

port = sys.argv[1];
cmd = sys.argv[2];

ser = serial.Serial()
ser.baudrate = 115200
ser.port = port;
ser.bytesize=8;
ser.parity='N';
ser.stopbits=1;
ser.timeout=1;
ser.xonxoff=0;
ser.rtscts=0;

ser.rts = False;
ser.dtr = False;

def readUntil(sentinel,includeNewline,timeout=3):
    buf = "";
    now = time.time();
    while(True):
        buf += ser.read()
        if (time.time() > now + timeout):
            raise RuntimeError("timeout");
        if (buf.find(sentinel) > 0):
            break;
    if (includeNewline): ser.readline();

def printOrd(subj):
    a = ""
    b = ""
    for c in subj:
        vis = c;
        if (ord(c) >= 128): vis = "";

        a += "  %s " % vis
        b += "%3d " % ord(c)
    print(a);
    print(b);

def main(ser):
    readUntil("ready",True);
    if (cmd == "factory"):
        ser.write("factory\n");
    if (cmd == "config"):
        ssid = sys.argv[3]
        passwd = sys.argv[4]
        ser.write("config:%s:%s\n" % (ssid,passwd));
    while(True):
        sys.stdout.write(ser.readline());

ser.open()
buf = "";
try:
    main(ser);
except KeyboardInterrupt:
    print("Terminated!");
    sys.exit(1)
except RuntimeError as er:
    print("Error!",er);
    sys.exit(1);
finally:
    ser.close()

sys.exit(0);

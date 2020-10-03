#!/usr/bin/env python

import serial
import sys
import time
import uuid;
import csv
import os
import re

port = sys.argv[1];

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

def main(ser):
    while True:
        sys.stdout.write(ser.read());

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

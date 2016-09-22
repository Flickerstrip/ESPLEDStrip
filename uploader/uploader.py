#!/usr/bin/env python

import serial
import sys
import time

ser = serial.Serial()
ser.baudrate = 115200
ser.port = '/dev/cu.usbserial-AL00P7CM'
ser.port = '/dev/cu.usbserial-AL00P7CN'
ser.port = '/dev/cu.usbserial-AL00P7CO'
ser.bytesize=8;
ser.parity='N';
ser.stopbits=1;
ser.timeout=1;
ser.xonxoff=0;
ser.rtscts=0;

ser.rts = False;
ser.dtr = False;

print "Opening serial.."
ser.open()
buf = "";
try:
    main(ser);
except KeyboardInterrupt:
    sys.exit()
finally:
    ser.close()


def main(ser):
    while(True):

#c = ser.read()
#sys.stdout.write(c);
#sys.stdout.flush();

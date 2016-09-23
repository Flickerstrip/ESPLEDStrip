#!/usr/bin/env python

import serial
import sys
import time
import uuid;
import csv
import os

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

def readUnitInformation():
    if (not os.path.isfile("units.csv")): return [];
    info = [];
    with open("units.csv", "rb") as csvfile:
        r = csv.reader(csvfile, delimiter=",", quotechar="'", quoting=csv.QUOTE_NONNUMERIC)
        for row in r:
            info.append({
                "mac":row[0],
                "uid":row[1],
                "batch":int(row[2]),
                "unit":int(row[3]),
                "time":int(row[4]),
            });
    return info;

def writeUnitInformation(info):
    with open("units.csv", "wb") as csvfile:
        w = csv.writer(csvfile, delimiter=",", quotechar="'", quoting=csv.QUOTE_NONNUMERIC)
        for unit in info:
            w.writerow([unit["mac"],unit["uid"],unit["batch"],unit["unit"],unit["time"]]);
            

def readUntil(sentinel,includeNewline):
    buf = "";
    while(True):
        buf += ser.read()
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

def validateResponse(thisEntry,s):
    if (s.find("identity: ") != 0): return False;
    s = s[len("identity: "):];
    s = s.split(" ");
    return s[0] == thisEntry["uid"] and int(s[1]) == thisEntry["batch"] and int(s[2]) == thisEntry["unit"];

def main(ser):
    info = readUnitInformation();

    readUntil("ready",True);
    ser.write("mac\n");
    ser.readline() #echoed line
    mac = ser.readline().strip(); #mac address

    thisEntry = None;
    for unit in info:
        if (unit["mac"] == mac):
            thisEntry = unit;

    if (not thisEntry):
        thisEntry = {
            "mac":mac,
            "uid":str(uuid.uuid4()),
            "batch":1,
            "unit":len(info)+1,
            "time":int(time.time())
        };
        info.append(thisEntry);
        print("Registered new Flickerstrip: %s" % thisEntry["uid"]);
    else:
        print("Entry Found: %s" % thisEntry["uid"]);
        thisEntry["time"] = int(time.time());

    ser.write("identify:%s,%s,%s\n" % (thisEntry["uid"],thisEntry["batch"],thisEntry["unit"]));
    ser.readline(); ser.readline(); #Skip over echoed response
    ser.write("checkidentity\n");
    ser.readline()
    checkme = ser.readline().strip()
    print("checkme: ",checkme);
    if validateResponse(thisEntry,checkme):
        print("valid!");
    else:
        print("invalid!");

    writeUnitInformation(info);


ser.open()
buf = "";
try:
    main(ser);
except KeyboardInterrupt:
    sys.exit()
finally:
    ser.close()

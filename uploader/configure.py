#!/usr/bin/env python

import serial
import sys
import time
import uuid;
import csv
import os
import re

batch = int(sys.argv[1]);
port = sys.argv[2];

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
                "note":row[5],
            });
    return info;

def writeUnitInformation(info):
    with open("units.csv", "wb") as csvfile:
        w = csv.writer(csvfile, delimiter=",", quotechar="'", quoting=csv.QUOTE_NONNUMERIC)
        for unit in info:
            w.writerow([unit["mac"],unit["uid"],unit["batch"],unit["unit"],unit["time"],unit["note"]]);
            

def readUntil(sentinel,includeNewline,timeout=3):
    buf = "";
    now = time.time();
    while(True):
        buf += ser.read()
        if (time.time() > now + timeout):
            raise RuntimeError("timeout while looking for '%s'" % sentinel);
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
    s = s.split(" ");
    return s[0] == thisEntry["uid"] and int(s[1]) == thisEntry["batch"] and int(s[2]) == thisEntry["unit"];

def main(ser):
    info = readUnitInformation();

    readUntil("ready",True);
    ser.write("mac\n");
    ser.readline() #echoed line
    mac = ser.readline().strip(); #mac address
    if not re.match("[0-9a-f]{1,2}([-:])[0-9a-f]{1,2}(\\1[0-9a-f]{1,2}){4}$", mac.lower()):
        raise RuntimeError("Failed to read mac address!",mac);

    thisEntry = None;
    for unit in info:
        if (unit["mac"] == mac):
            thisEntry = unit;

    if (thisEntry): index = info.index(thisEntry);

    if (thisEntry and index >= len(info)-2):
        print("Entry Found:");
        thisEntry["time"] = int(time.time());
        thisEntry["note"] = "reprogrammed";
    else:
        note = ""
        if thisEntry: note="duplicate mac"
        thisEntry = {
            "mac":mac,
            "uid":str(uuid.uuid4()),
            "batch":batch,
            "unit":len(info)+1,
            "time":int(time.time()),
            "note":note
        };
        info.append(thisEntry);
        print("Registered Flickerstrip:");

    print("   MAC: %s" % thisEntry["mac"]);
    print("   UID: %s" % thisEntry["uid"]);
    print(" BATCH: %s" % thisEntry["batch"]);
    print("  UNIT: %s" % thisEntry["unit"]);

    ser.write("identify:%s,%s,%s\n" % (thisEntry["uid"],thisEntry["batch"],thisEntry["unit"]));
    ser.readline(); ser.readline(); #Skip over echoed response
    ser.write("checkidentity\n");
    readUntil("identity: ",False);
    checkme = ser.readline().strip()
    if not validateResponse(thisEntry,checkme):
        raise RuntimeError("Validation failed");

    writeUnitInformation(info);

ser.open()
buf = "";
try:
    main(ser);
except KeyboardInterrupt:
    print("Terminated!");
    sys.exit(1)
except RuntimeError as er:
    raise er;
    sys.exit(1);
finally:
    ser.close()

sys.exit(0);

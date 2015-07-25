#ifndef NetworkManager_h
#define NetworkManager_h

#if defined(ARDUINO) && ARDUINO >= 100
#include "Arduino.h"
#else
#include "WProgram.h"
#endif

#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <WiFiServer.h>

class NetworkManager {
public:
  NetworkManager();
  void tick();

  void startUdp();
  bool isUdpActive();
  bool udpPacketAvailable();
  int getUdpPacket(IPAddress * ip, byte * buf, int len);

  void startTcp(IPAddress * ip, int port);
  bool isTcpActive();
  bool tcpPacketAvailable();
  int getTcpPacket(byte * buf, int len);
  WiFiClient * getTcp();

private:
  WiFiUDP udp;
  WiFiClient tcp;

  bool udpactive;
  bool tcpactive;

  int udpPacketLength;

  byte tcpBuffer[2000];
  int tcpIndex;
  int expectingBytes;
  bool tcpAvailable;

  void tcpTick();
};

#endif


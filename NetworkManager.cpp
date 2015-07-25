// vim:ts=2 sw=2:
#include "NetworkManager.h"

NetworkManager::NetworkManager() {

}

void NetworkManager::tick() {
  this->tcpTick();
}

void NetworkManager::startUdp() {
  Serial.println("listening for UDP packets");
  udp.begin(2836);
  this->udpactive = true;
}

bool NetworkManager::isUdpActive() {
  return this->udpactive;
}

bool NetworkManager::udpPacketAvailable() {
  if (this->udpPacketLength != 0) return true;

  this->udpPacketLength = udp.parsePacket();
}

int NetworkManager::getUdpPacket(IPAddress * remoteIp, byte * buf, int len) {
  if (this->udpPacketLength == 0) {
    this->udpPacketLength = udp.parsePacket();

    if (this->udpPacketLength == 0) return 0;
  }

  IPAddress ip = udp.remoteIP();
  memcpy(remoteIp,&ip,sizeof(IPAddress));

  int readLength = udp.read(buf, len);
  this->udpPacketLength = 0;

  return readLength;
}

void NetworkManager::startTcp(IPAddress * ip, int port) {
  Serial.print("Starting TCP connection: ");
  Serial.print(*ip);
  Serial.print(" : ");
  Serial.println(port);
  tcp.connect(*ip,port);

  this->tcpactive = true;
}

bool NetworkManager::isTcpActive() {
  return this->tcpactive;
}

void NetworkManager::tcpTick() {
  if(!tcp.connected()) this->tcpactive = false;
  if(!tcp.available()) return;

  if (this->tcpIndex == 0) {
    this->expectingBytes = tcp.read(); //little endian
    this->expectingBytes = this->expectingBytes | (tcp.read() << 8);
    this->expectingBytes | (tcp.read() << 16);
    this->expectingBytes | (tcp.read() << 24);

    Serial.print("Expecting bytes: ");
    Serial.println(this->expectingBytes);

    while(!tcp.available()) delay(0);
  }

  //Serial.print("Reading bytes... ");
  while(tcp.available()) {
    yield();
    char c = tcp.read();
    //Serial.print(c,HEX);
    //Serial.print(" ");
    this->tcpBuffer[this->tcpIndex++] = c;
    if (this->tcpIndex == this->expectingBytes) { //read until length is reached
      //Serial.println();
      this->tcpAvailable = true;
      break;
    }
  }
}

bool NetworkManager::tcpPacketAvailable() {
  return this->tcpAvailable;
}

int NetworkManager::getTcpPacket(byte * buf, int len) {
  memcpy(buf,this->tcpBuffer,min(this->expectingBytes,len));

  this->tcpIndex = 0;
  this->tcpAvailable = false;

  return this->expectingBytes;
}

WiFiClient * NetworkManager::getTcp() {
  return &tcp;
}

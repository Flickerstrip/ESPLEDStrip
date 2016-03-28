// vim:ts=2 sw=2:
#ifndef CaptivePortalConfigurator_h
#define CaptivePortalConfigurator_h

#if defined(ARDUINO) && ARDUINO >= 100
#include "Arduino.h"
#else
#include "WProgram.h"
#endif

#include <DNSServer.h>
#include <WiFiServer.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>

class CaptivePortalConfigurator {
public:
  CaptivePortalConfigurator(const char * ssid);
  void begin();
  bool hasConfiguration();
  void tick();

  String getSSID();
  String getPassword();

  void end();

private:
  const byte DNS_PORT = 53;
  const char* loginForm = "<html><head></head><body><form method='POST' action='/connect'><input name='ssid' type='text' placeholder='Network SSID'/><br/><input name='password' type='text' placeholder='Password'/><br/><input type='submit'></form></body></html>";

  bool configured;
  String ssid;
  String password;
  const char * configssid;

  IPAddress apIP;
  IPAddress netMsk;
  DNSServer dnsServer;
  ESP8266WebServer server;

  void handle_connect();
  void handle_redirect();
  void handle_showLogin();

  void urldecode2(char *dst, const char *src);
  String urlDecode(String s);
  void decodeParameters();
};

#endif



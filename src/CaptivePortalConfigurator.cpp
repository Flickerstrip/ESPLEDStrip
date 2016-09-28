// vim:ts=4 sw=4:
#include "CaptivePortalConfigurator.h"

CaptivePortalConfigurator::CaptivePortalConfigurator(const char * ssid) {
    this->configssid = ssid;
    this->apIP = IPAddress(192, 168, 1, 1);
    this->netMsk = IPAddress(255, 255, 255, 0);
    this->server = ESP8266WebServer(80);
}

void CaptivePortalConfigurator::begin() {
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(this->apIP, this->apIP, this->netMsk);
  WiFi.softAP(this->configssid);

  Serial.print("SSID: ");
  Serial.println(this->configssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.softAPIP());

  this->dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
  this->dnsServer.start(DNS_PORT, "*", this->apIP);

  this->server.on("/", [=](){return this->handle_redirect();});
  this->server.on("/generate_204", [=](){return this->handle_redirect();});  //Android captive portal
  this->server.on("/connect", HTTP_GET, [=](){return this->handle_showLogin();});
  this->server.on("/connect", HTTP_POST, [=](){return this->handle_connect();});
  this->server.onNotFound([=](){return this->handle_redirect();});
  this->server.begin();

  configured = false;
}

void CaptivePortalConfigurator::end() {
    this->dnsServer.stop();
    //TOOD stop server?
}

bool CaptivePortalConfigurator::hasConfiguration() {
    return configured;
}

void CaptivePortalConfigurator::tick() {
  this->dnsServer.processNextRequest();
  this->server.handleClient();
}

String CaptivePortalConfigurator::getSSID() {
    return this->ssid;
}

String CaptivePortalConfigurator::getPassword() {
    return this->password;
}

////////////////// Private Methods /////////////////////////
void CaptivePortalConfigurator::urldecode2(char *dst, const char *src) {
  char a, b;
  while (*src) {
    if ((*src == '%') && ((a = src[1]) && (b = src[2])) && (isxdigit(a) && isxdigit(b))) {
      if (a >= 'a')
        a -= 'a'-'A';
      if (a >= 'A')
        a -= ('A' - 10);
      else
        a -= '0';
      if (b >= 'a')
        b -= 'a'-'A';
      if (b >= 'A')
        b -= ('A' - 10);
      else
        b -= '0';
      *dst++ = 16*a+b;
      src+=3;
    } else if (*src == '+') {
      *src++;
      *dst++ = ' ';
    } else {
      *dst++ = *src++;
    }
  }
  *dst++ = '\0';
}

String CaptivePortalConfigurator::urlDecode(String s) {
  char src[s.length()*2];
  char dst[s.length()*2];
  s.toCharArray(src,s.length()*2);
  urldecode2(dst,src);
  return String(dst);
}

void CaptivePortalConfigurator::decodeParameters() {
  this->ssid = urlDecode(this->ssid);
  this->password = urlDecode(this->password);
}


void CaptivePortalConfigurator::handle_connect() {
  String toSend;
  String get_ssid = "";
  String get_password = "";
  for (uint8_t i=0; i<this->server.args(); i++){
    if (this->server.argName(i) == "ssid") {
      get_ssid = this->server.arg(i);
    } else if (this->server.argName(i) == "password") {
      get_password = this->server.arg(i);
    }
  }
  toSend += "Connecting...\n";
  this->server.send(200, "text/html", toSend);

  this->ssid = get_ssid;
  this->password = get_password;
  this->decodeParameters();
  configured = true;

  delay(50);
  this->end();
}

void CaptivePortalConfigurator::handle_redirect() {
  String redirectionUrl = "";
  redirectionUrl += "http://";
  for (int i=0; i<4; i++) {
    if (i != 0) redirectionUrl += ".";
    redirectionUrl += apIP[i];
  }
  redirectionUrl += "/connect";
  this->server.sendHeader("Location",redirectionUrl);
  this->server.send(302, "text/plain","Redirecting.." );
  delay(100);
}

void CaptivePortalConfigurator::handle_showLogin() {
  this->server.send(200, "text/html", this->loginForm);
  delay(100);
}

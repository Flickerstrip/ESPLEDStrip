#ifndef util_h
#define util_h

void *memchr(const void *s, int c, size_t n)
{
    unsigned char *p = (unsigned char*)s;
    while( n-- )
        if( *p != (unsigned char)c )
            p++;
        else
            return p;
    return 0;
}

const char* stristr(const char* str1, const char* str2 ) {
    const char* p1 = str1 ;
    const char* p2 = str2 ;
    const char* r = *p2 == 0 ? str1 : 0 ;

    while( *p1 != 0 && *p2 != 0 )
    {
        if( tolower( *p1 ) == tolower( *p2 ) )
        {
            if( r == 0 )
            {
                r = p1 ;
            }

            p2++ ;
        }
        else
        {
            p2 = str2 ;
            if( tolower( *p1 ) == tolower( *p2 ) )
            {
                r = p1 ;
                p2++ ;
            }
            else
            {
                r = 0 ;
            }
        }

        p1++ ;
    }

    return *p2 == 0 ? r : 0 ;
}

void urldecode(char *dst, int n, const char *src) {
  char a, b;
  char * start = dst;
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
    if (dst-start >= n-1) {
        *dst = '\0';
        return;   
    }
  }
  *dst++ = '\0';
}

unsigned long lastStart;
void start() {
  lastStart = millis();
}

void stop(String s) {
  Serial.print(s);
  Serial.print(": ");
  Serial.print(millis()-lastStart);
  Serial.print("ms");
  Serial.println();
}

void debugHex(const char *buf, int len) {
    for (int i=0; i<len; i++) {
        Serial.print(" ");
        if (buf[i] >= 32 && buf[i] <= 126) {
            Serial.print(buf[i]);
        } else {
            Serial.print(" ");
        }
        Serial.print(" ");
    }
    Serial.println();

    for (int i=0; i<len; i++) {
        if (buf[i] < 16) Serial.print("0");
        Serial.print(buf[i],HEX);
        Serial.print(" ");
    }
    Serial.println();
}

void waitForGo() {
    char buf[3];
    while(1) {
        while(Serial.available()) {
            buf[0] = buf[1];
            buf[1] = buf[2];
            char c = Serial.read();
            Serial.write(c);
            buf[2] = c;
            if (buf[0] == 'g' && buf[1] == 'o' && (buf[2] == '\n' || buf[2] == '\r')) return;
        }
    }

}

#endif

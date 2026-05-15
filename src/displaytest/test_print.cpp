/*#include <Arduino.h>
#include <Wire.h>
#include "displayclass/W32DL1414.h"

W32DL1414 display(0x40);

static void dumpStatus(const char* tag)
{
    W32DL1414RawStatus st = display.getStatusRaw();
    Serial.print(tag);
    Serial.print(" eng=");
    Serial.print(st.engine_status);
    Serial.print(" op=");
    Serial.print(st.operation_status);
    Serial.print(" err=");
    Serial.print(st.error_code);
    Serial.print(" job=");
    Serial.print(st.job_status);
    Serial.print(" type=");
    Serial.print(st.job_type);
    Serial.print(" off=");
    Serial.print(st.job_offset);
    Serial.print(" prog=");
    Serial.println(st.job_progress);
}

static void makeString(char* out, uint8_t len, char ch)
{
    for (uint8_t i = 0; i < len; ++i) out[i] = ch;
    out[len] = '\0';
}

static void oneCase(uint8_t len, bool usePrintf)
{
    if (len>120) {  // sanity check for makeString buffer size
        Serial.println("len too large");
        return;
    }

    char s[128];
    char c = (char)('A' + (len % 26));
    makeString(s, len, c);

    display.clear(' ');
    delay(20);
    display.waitUntilIdle(8000);
    delay(20);

    Serial.print("LEN=");
    Serial.print(len);
    Serial.print(" MODE=");
    Serial.println(usePrintf ? "printf" : "print");

    dumpStatus(" before:");

    if (usePrintf) {
        display.printf(0, "%.02d*'%c': %s", len, c, s);
    } else {
        display.print(64, s);
    }

    delay(30);
    dumpStatus(" after call:");

    bool ok = display.flush();
    Serial.print(" flush=");
    Serial.println(ok ? "OK" : "FAIL");

    delay(30);
    dumpStatus(" after flush:");

    Serial.print(" text=");
    Serial.println(s);
    Serial.println();
}

void setup()
{
    Serial.begin(115200);
    Wire.begin();
    Wire.setClock(100000);
    delay(1000);

    if (!display.begin()) {
        Serial.println("begin failed");
        return;
    }

    for (uint8_t len = 1; len <= 120; ++len) {
        //oneCase(len, false);
        //delay(1000);
        oneCase(len, true);
        delay(500);
    }
}

void loop()
{
}*/
/*#include <Arduino.h>
#include <Wire.h>
#include "displayclass/W32DL1414.h"

W32DL1414 display(0x40);

static void printRawStatus(const char* label)
{
    W32DL1414RawStatus st = display.getStatusRaw();
    Serial.print(label);
    Serial.print(" job_status=0x");
    Serial.print(st.job_status, HEX);
    Serial.print(", job_type=0x");
    Serial.print(st.job_type, HEX);
    Serial.print(", offset=");
    Serial.print(st.job_offset);
    Serial.print(", progress=");
    Serial.println(st.job_progress);
}

void setup()
{
    Serial.begin(115200);
    Wire.begin();
    delay(1000);

    Serial.println("=== W32DL1414 Integrationstest ===");

    if (!display.begin()) {
        Serial.println("begin() failed");
        return;
    }

    Serial.print("Version: ");
    Serial.print(display.getFirmwareMajorVersion());
    Serial.print('.');
    Serial.print(display.getFirmwareMinorVersion());
    Serial.print('.');
    Serial.println(display.getFirmwarePatchVersion());

    uint8_t ping = display.ping();
    Serial.print("PING: 0x");
    Serial.println(ping, HEX);
    if (ping != 0xAA) {
        Serial.println("PING failed");
        return;
    }

    W32DL1414SystemInfo sys = display.getSystemInfo();
    Serial.print("System: board=");
    Serial.print(sys.board==W32DL1414_BOARD_ID_ATTINY44 ? "ATTINY44" :
                 sys.board==W32DL1414_BOARD_ID_ATTINY84 ? "ATTINY84" : "UNKNOWN");
    Serial.print(", clock=");
    Serial.print(sys.clock_khz);
    Serial.print("kHz, ram_free=");
    Serial.print(sys.ram_free);
    Serial.print(", ram_total=");
    Serial.println(sys.ram_total);

    bool ok = display.writeChar(0, 'H');
    Serial.print("WRITE_CHAR: ");
    Serial.println(ok ? "OK" : "FAIL");

    uint8_t test_buf[] = {'T', 'E', 'S', 'T'};
    ok = display.writeBuffer(4, test_buf, 4);
    Serial.print("WRITE_BUF: ");
    Serial.println(ok ? "OK" : "FAIL");

    printRawStatus("Before CLEAR:");
    display.clear(' ');
    printRawStatus("After CLEAR start:");

    bool clear_done = display.waitUntilIdle(1500);
    Serial.print("CLEAR wait: ");
    Serial.println(clear_done ? "DONE" : "TIMEOUT/FAIL");
    printRawStatus("After CLEAR wait:");

    // Test 4: Text schreiben und anzeigen
    display.writeChar(0, 'W');
    display.writeChar(1, '3');
    display.writeChar(2, '2');
    display.writeChar(3, '!');
    Serial.println("âœ… WRITE_CHAR Text OK");

    Serial.println("FLUSH start...");
    printRawStatus("Before FLUSH:");
    bool flush_done = display.flush();
    Serial.print("FLUSH result: ");
    Serial.println(flush_done ? "DONE" : "FAIL");
    printRawStatus("After FLUSH:");

    display.clear(' ');
    bool clear2_done = display.waitUntilIdle(1500);
    Serial.print("CLEAR2 wait: ");
    Serial.println(clear2_done ? "DONE" : "TIMEOUT/FAIL");

    display.writeChar(0, 'A');
    display.writeChar(1, 'B');
    display.writeChar(2, 'C');
    display.writeChar(3, 'D');

    bool flush2_done = display.flush();
    Serial.print("FLUSH2 result: ");
    Serial.println(flush2_done ? "DONE" : "FAIL");

    display.clear('*');
    bool clear_done3 = display.waitUntilIdle(1500);
    Serial.print("CLEAR3 wait: ");
    Serial.println(clear_done3 ? "DONE" : "TIMEOUT/FAIL");

    uint8_t buf1[] = {'T', 'E', 'S', 'T'};
    bool ok1 = display.writeBuffer(0, buf1, 4);
    Serial.print("WRITE_BUF(0,\"TEST\"): ");
    Serial.println(ok1 ? "OK" : "FAIL");

    bool flush3_done = display.flush();
    Serial.print("FLUSH3 result: ");
    Serial.println(flush3_done ? "DONE" : "FAIL");

    display.clear(' ');
    bool clear_done4 = display.waitUntilIdle(1500);
    Serial.print("FULL CLEAR wait: ");
    Serial.println(clear_done ? "DONE" : "FAIL");

    // 28 Bytes "12345678901234567890123456789012" 
    uint8_t full_buf[28];
    for (uint8_t i = 0; i < 28; i++) {
        full_buf[i] = 'A' + (i % 26);  // "01234567890123456789012345678901"
    }
    bool full_ok = display.writeBuffer(0, full_buf, 28) && display.writeBuffer(28, full_buf, 28) && display.writeBuffer(56, full_buf, 28)
                    && display.writeBuffer(84, full_buf, 28) && display.writeBuffer(112, full_buf, 16);
    Serial.print("FULL WRITE_BUF(28): ");
    Serial.println(full_ok ? "OK" : "FAIL");

    bool full_flush = display.flush();
    Serial.print("FULL FLUSH result: ");
    Serial.println(full_flush ? "DONE" : "FAIL");

    display.print(0, "1234567890123456789012345678901234567890"); // 40 Zeichen â†’ 2 Chunks
    
    display.printf(0, "POS=%u VALUE=%d WUTHCKE *** KARL FERDINAND & MAGDA & ANDRE WUTHCKE", 40, 12345); // Format Ã¼ber Grenze
    display.flush();

    Serial.println("=== Test ENDE ===");
}

void loop()
{
        static uint8_t cursor = 0;

    if (Serial.available() > 0) {
        String line = Serial.readStringUntil('\n');
        line.trim();  // entfernt \r und Leerraum am Ende/Anfang

        if (line.length() == 0) {
            return;
        }

        if (cursor >= W32DL1414_DISPLAY_SIZE) {
            cursor = 0;
        }

        display.print(cursor, line.c_str());
        bool ok = display.flush();

        Serial.print("WRITE @");
        Serial.print(cursor);
        Serial.print(": ");
        Serial.println(ok ? "OK" : "FAIL");

        if (ok) {
            uint16_t next = cursor + line.length();
            cursor = (next < W32DL1414_DISPLAY_SIZE) ? next : W32DL1414_DISPLAY_SIZE;
        }
    }
}*/
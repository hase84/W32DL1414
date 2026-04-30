/*#include <Arduino.h>
#include <Wire.h>
#include "displayclass/W32DL1414.h"

W32DL1414 display(0x40);

const char* engine_status_emoji(uint8_t s) {
    switch (s) {
        case 0: return "READY";
        case 1: return "BUSY ";
        case 2: return "ERROR";
        default: return "?";
    }
}

const char* op_status_emoji(uint8_t s) {
    switch (s) {
        case 0: return "IDLE ";
        case 1: return "RUN  ";
        case 2: return "OK   ";
        case 3: return "FAIL ";
        default: return "?";
    }
}

const char* error_symbol(uint8_t e) {
    switch (e) {
        case 0x00: return "OK ";
        case 0xE0: return "IOP";
        case 0xE1: return "IAR";
        case 0xE2: return "OOR";
        case 0xE3: return "BSY";
        case 0xE4: return "INT";
        case 0xE5: return "OVF";
        case 0xE6: return "NIM";
        default: return "?";
    }
}

const char* job_status_emoji(uint8_t s) {
    switch (s) {
        case 0: return "NONE";
        case 1: return "PEND";
        case 2: return "RUN ";
        case 3: return "DONE";
        case 4: return "FAIL";
        default: return "?";
    }
}

struct TestResult {
    const char* name;
    bool success;
    uint32_t duration_ms;
    W32DL1414RawStatus before;
    W32DL1414RawStatus after;
};

static TestResult run_test(const char* name, bool (*test_func)()) {
    TestResult res = {name, false, 0, {0}, {0}};
    res.before = display.getStatusRaw();
    uint32_t start = millis();
    res.success = test_func();
    res.duration_ms = millis() - start;
    res.after = display.getStatusRaw();
    return res;
}

bool test_begin() { return display.begin(); }
bool test_ping() { return display.ping() == 0xAA; }
bool test_version() {
    return display.getFirmwareMajorVersion() == 0 &&
           display.getFirmwareMinorVersion() == 2 &&
           display.getFirmwarePatchVersion() >= 2;
}
bool test_sysinfo() {
    W32DL1414SystemInfo sys = display.getSystemInfo();
    return sys.board == 2 && sys.clock_khz == 8000 && sys.ram_total == 512 && sys.ram_free > 100;
}
bool test_write_char() { return display.writeChar(0, 'X'); }
bool test_write_buffer() {
    const uint8_t buf[] = { 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H' };
    return display.writeBuffer(8, buf, sizeof(buf));
}
bool test_clear() {
    display.clear(' ');
    return display.waitUntilIdle(8000);
}
bool test_flush() {
    display.print(0, "HELLO WORLD TEST");
    return display.flush();
}
bool test_cursor_set_get() {
    display.setCursor(42);
    return display.getCursor() == 42;
}

bool ensure_idle(uint16_t timeout_ms = 8000) {
    return display.waitUntilIdle(timeout_ms);
}

bool test_print_basic() {
    if (!ensure_idle(30000)) return false;
    display.clear(' ');
    if (!ensure_idle(30000)) return false;
    display.setCursor(0);
    display.print(0, "PRINT TEST");
    bool ok = display.flush();
    if (!ensure_idle(30000)) return false;
    return ok;
}

bool test_printf() {
    if (!ensure_idle(30000)) return false;
    display.clear(' ');
    if (!ensure_idle(30000)) return false;
    display.setCursor(0);
    display.printf(0, "NUM=%d POS=%u", 123, 456);
    bool ok = display.flush();
    if (!ensure_idle(30000)) return false;
    return ok;
}

void format_state(char* out, size_t out_size, const W32DL1414RawStatus& st) {
    snprintf(out, out_size, "%s/%s/%s/%s p%-3u",
        engine_status_emoji(st.engine_status),
        op_status_emoji(st.operation_status),
        error_symbol(st.error_code),
        job_status_emoji(st.job_status),
        st.job_progress);
}

void print_results_table(const TestResult results[], size_t count) {
    Serial.println();
    Serial.println("=== COMPREHENSIVE TEST RESULTS v0.2.2 ===");
    Serial.println("+----------------------+---------+----------+---------------------------+---------------------------+");
    Serial.println("| Test                 | Result  | Time     | Before                    | After                     |");
    Serial.println("+----------------------+---------+----------+---------------------------+---------------------------+");

    for (size_t i = 0; i < count; ++i) {
        const TestResult& r = results[i];
        char before_str[32];
        char after_str[32];
        format_state(before_str, sizeof(before_str), r.before);
        format_state(after_str, sizeof(after_str), r.after);

        Serial.printf("| %-20s | %-7s | %6lums | %-25s | %-25s |\n",
            r.name,
            r.success ? "PASS" : "FAIL",
            r.duration_ms,
            before_str,
            after_str);
    }

    uint32_t total_time = 0;
    uint32_t pass_count = 0;
    for (size_t i = 0; i < count; ++i) {
        total_time += results[i].duration_ms;
        if (results[i].success) ++pass_count;
    }

    Serial.println("+----------------------+---------+----------+---------------------------+---------------------------+");
    Serial.printf("TOTAL: %lu/%lu PASS (%.1f%%) | AVG: %lums\n",
        (unsigned long)pass_count,
        (unsigned long)count,
        count ? (100.0f * pass_count / count) : 0.0f,
        count ? (unsigned long)(total_time / count) : 0UL);
    Serial.println();
    Serial.println("Legend:");
    Serial.println("  Engine: READY BUSY ERROR");
    Serial.println("  Op    : IDLE RUN OK FAIL");
    Serial.println("  Error : OK IOP IAR OOR BSY INT OVF NIM");
    Serial.println("  Job   : NONE PEND RUN DONE FAIL");
}

void setup() {
    Serial.begin(115200);
    while (!Serial) {}

    Wire.begin();
    Wire.setClock(100000);
    delay(2000);

    TestResult results[] = {
        run_test("begin", test_begin),
        run_test("ping", test_ping),
        run_test("version", test_version),
        run_test("sysinfo", test_sysinfo),
        run_test("write_char(0,'X')", test_write_char),
        run_test("write_buffer(8)", test_write_buffer),
        run_test("clear + wait", test_clear),
        run_test("flush 'HELLO'", test_flush),
        run_test("cursor 42", test_cursor_set_get),
        run_test("print 'PRINT'", test_print_basic),
        run_test("printf num/pos", test_printf),
    };

    print_results_table(results, sizeof(results) / sizeof(results[0]));

    Serial.println();
    Serial.println("Visual check:");
    Serial.println("  after flush test  : HELLO WORLD TEST");
    Serial.println("  after print test  : PRINT TEST");
    Serial.println("  after printf test : NUM=123 POS=456");
}

void loop() {}*/
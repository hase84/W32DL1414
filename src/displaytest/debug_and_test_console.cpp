/*#include <Arduino.h>
#include <Wire.h>
#include <stdarg.h>
#include "displayclass/W32DL1414.h"

W32DL1414 display(0x40);

struct StatusSnapshot {
    const char* label;
    W32DL1414RawStatus st;
};

struct TestResult {
    const char* name;
    bool success;
    uint32_t duration_ms;
    char detail[96];
};

struct TestEntry {
    char key;
    const char* name;
    bool reads_only;
    bool (*fn)(char*, size_t);
};

void print_status_block(const char* title, const W32DL1414RawStatus& st);
void print_snapshot(const StatusSnapshot& s);
void print_trace(const char* title, const StatusSnapshot* trace, size_t count);
bool wait_idle(uint16_t timeout_ms = 8000);
bool prepare_display_test(const char* name, StatusSnapshot* trace = nullptr, size_t trace_count = 0);
void print_help();
void handle_serial();
void run_all_tests();
void run_named_test(const TestEntry& t);
void print_test_result(const char* title, const TestResult& r);

bool test_begin(char* detail, size_t detail_size);
bool test_ping(char* detail, size_t detail_size);
bool test_version(char* detail, size_t detail_size);
bool test_sysinfo(char* detail, size_t detail_size);
bool test_write_char(char* detail, size_t detail_size);
bool test_write_buffer(char* detail, size_t detail_size);
bool test_clear(char* detail, size_t detail_size);
bool test_flush(char* detail, size_t detail_size);
bool test_cursor_set_get(char* detail, size_t detail_size);
bool test_print_basic(char* detail, size_t detail_size);
bool test_printf(char* detail, size_t detail_size);
bool test_long_printf(char* detail, size_t detail_size);
bool test_fill_then_clear(char* detail, size_t detail_size);
bool test_status(char* detail, size_t detail_size);

const char* yesno(bool ok) { return ok ? "PASS" : "FAIL"; }

const char* engine_status_text(uint8_t s) {
    switch (s) {
        case 0: return "READY";
        case 1: return "BUSY ";
        case 2: return "ERROR";
        default: return "?";
    }
}

const char* op_status_text(uint8_t s) {
    switch (s) {
        case 0: return "IDLE";
        case 1: return "RUN ";
        case 2: return "OK  ";
        case 3: return "FAIL";
        default: return "?";
    }
}

const char* error_text(uint8_t e) {
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

const char* job_status_text(uint8_t s) {
    switch (s) {
        case 0: return "NONE";
        case 1: return "PEND";
        case 2: return "RUN ";
        case 3: return "DONE";
        case 4: return "FAIL";
        default: return "?";
    }
}

void format_state(char* out, size_t out_size, const W32DL1414RawStatus& st) {
    snprintf(out, out_size, "%s | %s | %s | %s | off=%u prog=%u",
        engine_status_text(st.engine_status),
        op_status_text(st.operation_status),
        error_text(st.error_code),
        job_status_text(st.job_status),
        st.job_offset,
        st.job_progress);
}

void print_status_block(const char* title, const W32DL1414RawStatus& st) {
    char line[96];
    format_state(line, sizeof(line), st);
    Serial.println();
    Serial.printf("=== %s ===\n", title);
    Serial.println(line);
    Serial.printf("job_type=%u\n", st.job_type);
}

void print_snapshot(const StatusSnapshot& s) {
    char line[96];
    format_state(line, sizeof(line), s.st);
    Serial.printf("%-18s | %s | job=%u type=%u\n",
        s.label,
        line,
        s.st.job_status,
        s.st.job_type);
}

void print_trace(const char* title, const StatusSnapshot* trace, size_t count) {
    Serial.println();
    Serial.printf("=== %s ===\n", title);
    Serial.println("label              | engine/op/error/job/off/prog                        | job type");
    Serial.println("--------------------------------------------------------------------------------");
    for (size_t i = 0; i < count; ++i) {
        print_snapshot(trace[i]);
    }
    Serial.println("--------------------------------------------------------------------------------");
}

bool wait_idle(uint16_t timeout_ms) {
    return display.waitUntilIdle(timeout_ms);
}

bool prepare_display_test(const char* name, StatusSnapshot* trace, size_t trace_count) {
    (void)name;

    if (trace && trace_count >= 1) trace[0] = {"prepare-start", display.getStatusRaw()};

    if (!wait_idle(30000)) {
        if (trace && trace_count >= 2) trace[1] = {"not-idle", display.getStatusRaw()};
        return false;
    }

    if (trace && trace_count >= 2) trace[1] = {"after-wait", display.getStatusRaw()};

    display.clear(' ');

    if (trace && trace_count >= 3) trace[2] = {"after-clear", display.getStatusRaw()};

    if (!wait_idle(30000)) {
        if (trace && trace_count >= 4) trace[3] = {"clear-timeout", display.getStatusRaw()};
        return false;
    }

    if (trace && trace_count >= 4) trace[3] = {"idle-ready", display.getStatusRaw()};
    return true;
}

bool test_begin(char* detail, size_t detail_size) {
    bool ok = display.begin();
    snprintf(detail, detail_size, "begin()=%s", yesno(ok));
    return ok;
}

bool test_ping(char* detail, size_t detail_size) {
    uint8_t v = display.ping();
    snprintf(detail, detail_size, "ping=0x%02X", v);
    return v == 0xAA;
}

bool test_version(char* detail, size_t detail_size) {
    uint8_t maj = display.getFirmwareMajorVersion();
    uint8_t min = display.getFirmwareMinorVersion();
    uint8_t pat = display.getFirmwarePatchVersion();
    snprintf(detail, detail_size, "v%u.%u.%u", maj, min, pat);
    return maj == 0 && min == 2 && pat >= 2;
}

bool test_sysinfo(char* detail, size_t detail_size) {
    W32DL1414SystemInfo sys = display.getSystemInfo();
    snprintf(detail, detail_size, "board=%u clk=%ukHz ram=%u/%u",
        sys.board, sys.clock_khz, sys.ram_free, sys.ram_total);
    return sys.board == 2 && sys.clock_khz == 8000 && sys.ram_total == 512 && sys.ram_free > 100;
}

bool test_write_char(char* detail, size_t detail_size) {
    if (!prepare_display_test("write_char", nullptr, 0)) {
        snprintf(detail, detail_size, "prepare failed");
        return false;
    }

    bool ok = display.writeChar(0, 'X');
    if (ok) ok = display.flush();
    if (ok) ok = wait_idle(30000);

    snprintf(detail, detail_size, "pos=0 ch='X' + flush");
    return ok;
}

bool test_write_buffer(char* detail, size_t detail_size) {
    if (!prepare_display_test("write_buffer", nullptr, 0)) {
        snprintf(detail, detail_size, "prepare failed");
        return false;
    }

    const uint8_t buf[] = {'A','B','C','D','E','F','G','H'};
    bool ok = display.writeBuffer(8, buf, sizeof(buf));
    if (ok) ok = display.flush();
    if (ok) ok = wait_idle(30000);

    snprintf(detail, detail_size, "pos=8 len=%u + flush", (unsigned)sizeof(buf));
    return ok;
}

bool test_clear(char* detail, size_t detail_size) {
    if (!wait_idle(30000)) {
        snprintf(detail, detail_size, "not idle before clear");
        return false;
    }

    display.clear(' ');
    bool ok = display.flush();
    if (ok) ok = wait_idle(8000);

    snprintf(detail, detail_size, "clear(' ') + flush");
    return ok;
}

bool test_flush(char* detail, size_t detail_size) {
    StatusSnapshot tr[4] = {0};

    if (!prepare_display_test("flush", tr, 4)) {
        print_trace("flush prepare trace", tr, 4);
        snprintf(detail, detail_size, "prepare failed");
        return false;
    }

    display.print(0, "HELLO WORLD TEST");
    bool ok = display.flush();
    if (ok) ok = wait_idle(30000);

    print_trace("flush trace", tr, 4);
    snprintf(detail, detail_size, "print+flush HELLO WORLD TEST");
    return ok;
}

bool test_cursor_set_get(char* detail, size_t detail_size) {
    uint8_t old = display.getCursor();
    display.setCursor(42);
    uint8_t now = display.getCursor();
    snprintf(detail, detail_size, "cursor %u -> %u", old, now);
    return now == 42;
}

bool test_print_basic(char* detail, size_t detail_size) {
    StatusSnapshot tr[4] = {0};

    if (!prepare_display_test("print", tr, 4)) {
        print_trace("print prepare trace", tr, 4);
        snprintf(detail, detail_size, "prepare failed");
        return false;
    }

    display.print(0, "PRINT TEST");
    bool ok = display.flush();
    if (ok) ok = wait_idle(30000);

    print_trace("print trace", tr, 4);
    snprintf(detail, detail_size, "print(0,\"PRINT TEST\") + flush");
    return ok;
}

bool test_printf(char* detail, size_t detail_size) {
    StatusSnapshot tr[4] = {0};

    if (!prepare_display_test("printf", tr, 4)) {
        print_trace("printf prepare trace", tr, 4);
        snprintf(detail, detail_size, "prepare failed");
        return false;
    }

    display.printf(0, "NUM=%d POS=%u", 123, 456);
    bool ok = display.flush();
    if (ok) ok = wait_idle(30000);

    print_trace("printf trace", tr, 4);
    snprintf(detail, detail_size, "printf(NUM=123 POS=456) + flush");
    return ok;
}

bool test_long_printf(char* detail, size_t detail_size) {
    StatusSnapshot tr[4] = {0};

    if (!prepare_display_test("long_printf", tr, 4)) {
        print_trace("long_printf prepare trace", tr, 4);
        snprintf(detail, detail_size, "prepare failed");
        return false;
    }

    display.printf(0,
        "LEN=%d MODE=%s TEXT=%s",
        120,
        "PRINTF",
        "^!\"\\/$%&()=?'#*+-.,:;-_<>[]0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ@*-*-*-*-*-*-*@@-*-*-*-*-*-*-*-*-*........");

    bool ok = display.flush();
    if (ok) ok = wait_idle(30000);

    print_trace("long_printf trace", tr, 4);
    snprintf(detail, detail_size, "long printf + flush");
    return ok;
}

bool test_fill_then_clear(char* detail, size_t detail_size) {
    StatusSnapshot tr[4] = {0};

    if (!prepare_display_test("fill_then_clear", tr, 4)) {
        print_trace("fill_then_clear prepare trace", tr, 4);
        snprintf(detail, detail_size, "prepare failed");
        return false;
    }

    display.print(0, "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX");
    bool ok = display.flush();
    if (ok) ok = wait_idle(30000);

    delay(1000);

    display.clear(' ');
    if (ok) ok = wait_idle(30000);

    print_trace("fill_then_clear trace", tr, 4);
    snprintf(detail, detail_size, "fill->flush->clear");
    return ok;
}

bool test_status(char* detail, size_t detail_size) {
    W32DL1414RawStatus st = display.getStatusRaw();
    snprintf(detail, detail_size, "eng=%u op=%u err=%u job=%u type=%u off=%u prog=%u",
        st.engine_status, st.operation_status, st.error_code, st.job_status, st.job_type, st.job_offset, st.job_progress);
    return true;
}

TestEntry tests[] = {
    {'1', "begin",        true,  test_begin},
    {'2', "ping",         true,  test_ping},
    {'3', "version",      true,  test_version},
    {'4', "sysinfo",      true,  test_sysinfo},
    {'5', "write_char",   false, test_write_char},
    {'6', "write_buffer", false, test_write_buffer},
    {'7', "clear",        false, test_clear},
    {'8', "flush",        false, test_flush},
    {'9', "cursor",       true,  test_cursor_set_get},
    {'p', "print",        false, test_print_basic},
    {'f', "printf",       false, test_printf},
    {'l', "long_printf",  false, test_long_printf},
    {'k', "fill_clear",   false, test_fill_then_clear},
    {'s', "status",       true,  test_status},
};

void print_test_result(const char* title, const TestResult& r) {
    Serial.println();
    Serial.println("+------------------------------------------------------------------------------------------------+");
    Serial.printf("| %-16s | %-7s | %6ums | %-24s |\n",
        title,
        r.success ? "PASS" : "FAIL",
        r.duration_ms,
        r.detail);
    Serial.println("+------------------------------------------------------------------------------------------------+");
}

void run_named_test(const TestEntry& t) {
    TestResult r = {t.name, false, 0, {0}};
    uint32_t start = millis();
    r.success = t.fn(r.detail, sizeof(r.detail));
    r.duration_ms = millis() - start;
    print_test_result(t.name, r);
}

void run_all_tests() {
    Serial.println();
    Serial.println("Running full test suite...");
    for (size_t i = 0; i < sizeof(tests) / sizeof(tests[0]); ++i) {
        run_named_test(tests[i]);
    }
}

void print_help() {
    Serial.println();
    Serial.println("======================================================");
    Serial.println(" W32DL1414 TEST & DEBUG CONSOLE");
    Serial.println("======================================================");
    Serial.println(" General:");
    Serial.println("   h  help");
    Serial.println("   a  run all tests");
    Serial.println("   s  show raw status");
    Serial.println();
    Serial.println(" Read tests:");
    Serial.println("   1  begin");
    Serial.println("   2  ping");
    Serial.println("   3  version");
    Serial.println("   4  sysinfo");
    Serial.println("   9  cursor set/get");
    Serial.println();
    Serial.println(" Write tests:");
    Serial.println("   5  write_char + flush");
    Serial.println("   6  write_buffer + flush");
    Serial.println("   7  clear + flush");
    Serial.println("   8  print + flush (HELLO WORLD TEST)");
    Serial.println("   p  print + flush (PRINT TEST)");
    Serial.println("   f  printf + flush");
    Serial.println("   l  long printf + flush");
    Serial.println("   k  fill + flush + clear");
    Serial.println("======================================================");
}

void handle_serial() {
    while (Serial.available() > 0) {
        char cmd = (char)Serial.read();
        if (cmd == '\r' || cmd == '\n') continue;

        Serial.println();
        Serial.printf(">> command = '%c'\n", cmd);

        if (cmd == 'h') {
            print_help();
            continue;
        }

        if (cmd == 'a') {
            run_all_tests();
            continue;
        }

        bool handled = false;
        for (size_t i = 0; i < sizeof(tests) / sizeof(tests[0]); ++i) {
            if (cmd == tests[i].key) {
                if (cmd == 's') {
                    print_status_block("RAW STATUS", display.getStatusRaw());
                } else {
                    run_named_test(tests[i]);
                }
                handled = true;
                break;
            }
        }

        if (!handled) {
            Serial.println("Unknown command. Press 'h' for help.");
        }
    }
}

void setup() {
    Serial.begin(115200);
    while (!Serial) {}

    Wire.begin();
    Wire.setClock(100000);
    delay(2000);

    Serial.println();
    Serial.println("Booting W32DL1414 debug console...");
    bool ok = display.begin();
    Serial.printf("Display begin: %s\n", yesno(ok));
    print_status_block("INITIAL STATUS", display.getStatusRaw());
    print_help();
}

void loop() {
    handle_serial();
}*/
#include <Arduino.h>
#include <Wire.h>
#include "displayclass/W32DL1414.h"

#define USE_PLAIN_ASCII 1

W32DL1414 display(0x40);

struct ProbeSnapshot {
    bool i2c_ok;
    uint8_t i2c_ping;
    W32DL1414SystemInfo sys;
    W32DL1414RawStatus raw;
};

struct AssertionResult {
    const char* label;
    bool ok;
    char expected[40];
    char actual[40];
};

struct TestReport {
    const char* name;
    bool success;
    uint32_t duration_ms;
    ProbeSnapshot before;
    ProbeSnapshot fail;
    ProbeSnapshot after;
    bool has_fail_snapshot;
    AssertionResult checks[16];
    uint8_t check_count;
    char note[96];
};

typedef bool (*TestActionFn)(TestReport& r);

struct TestSpec {
    char key;
    const char* name;
    bool reads_only;
    TestActionFn action;
};

const char* engine_status_text(uint8_t s) {
    switch (s) {
        case 0: return "READY";
        case 1: return "BUSY";
        case 2: return "ERROR";
        default: return "?";
    }
}

const char* op_status_text(uint8_t s) {
    switch (s) {
        case 0: return "IDLE";
        case 1: return "RUN";
        case 2: return "OK";
        case 3: return "FAIL";
        default: return "?";
    }
}

const char* error_text(uint8_t e) {
    switch (e) {
        case 0x00: return "OK";
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
        case 2: return "RUN";
        case 3: return "DONE";
        case 4: return "FAIL";
        default: return "?";
    }
}

void format_state(char* out, size_t out_size, const W32DL1414RawStatus& st) {
    snprintf(out, out_size,
        "engine=%s op=%s error=%s job=%s offset=%u progress=%u%%",
        engine_status_text(st.engine_status),
        op_status_text(st.operation_status),
        error_text(st.error_code),
        job_status_text(st.job_status),
        st.job_offset,
        st.job_progress);
}

void print_probe_line(const char* prefix, const ProbeSnapshot& p) {
    Serial.printf("  %si2c     ping=0x%02X ok=%s\n", prefix, p.i2c_ping, p.i2c_ok ? "true" : "false");
    Serial.printf("  %ssystem  board=%u clk=%ukHz ram=%u/%u\n",
        prefix, p.sys.board, p.sys.clock_khz, p.sys.ram_free, p.sys.ram_total);
    Serial.printf("  %sengine  engine=%s op=%s error=%s job=%s type=%u offset=%u progress=%u%%\n",
        prefix,
        engine_status_text(p.raw.engine_status),
        op_status_text(p.raw.operation_status),
        error_text(p.raw.error_code),
        job_status_text(p.raw.job_status),
        p.raw.job_type,
        p.raw.job_offset,
        p.raw.job_progress);
}

void print_status_block(const char* title, const W32DL1414RawStatus& st) {
    char line[128];
    format_state(line, sizeof(line), st);
    Serial.println();
    Serial.printf("=== %s ===\n", title);
    Serial.println(line);
    Serial.printf("job_type=%u\n", st.job_type);
}

bool wait_idle(uint16_t timeout_ms = 8000) {
    return display.waitUntilIdle(timeout_ms);
}

bool wait_stable_idle(uint16_t timeout_ms = 8000, uint16_t settle_ms = 20) {
    if (!display.waitUntilIdle(timeout_ms)) return false;
    delay(settle_ms);
    W32DL1414RawStatus st = display.getStatusRaw();
    return st.job_status == 3 && st.operation_status == 2 && st.error_code == 0x00;
}

bool prepare_display_test(const char* name) {
    (void)name;
//   if (!wait_stable_idle(30000, 20)) return false;
//  display.clear(' ');
    if (!wait_stable_idle(30000, 20)) return false;
    return true;
}

bool capture_probe(ProbeSnapshot& p) {
    p.i2c_ping = display.ping();
    p.i2c_ok = (p.i2c_ping == 0xAA);
    p.sys = display.getSystemInfo();
    p.raw = display.getStatusRaw();
    return true;
}

void capture_fail_snapshot(TestReport& r) {
    if (!r.has_fail_snapshot) {
        capture_probe(r.fail);
        r.has_fail_snapshot = true;
    }
}

void add_check(TestReport& r, const char* label, bool ok, const char* expected, const char* actual) {
    if (r.check_count >= sizeof(r.checks) / sizeof(r.checks[0])) return;
    AssertionResult& c = r.checks[r.check_count++];
    c.label = label;
    c.ok = ok;
    strncpy(c.expected, expected, sizeof(c.expected) - 1);
    c.expected[sizeof(c.expected) - 1] = '\0';
    strncpy(c.actual, actual, sizeof(c.actual) - 1);
    c.actual[sizeof(c.actual) - 1] = '\0';
}

void add_check_eq_u8(TestReport& r, const char* label, uint8_t expect, uint8_t actual) {
    char exp[40], act[40];
    snprintf(exp, sizeof(exp), "0x%02X", expect);
    snprintf(act, sizeof(act), "0x%02X", actual);
    add_check(r, label, expect == actual, exp, act);
}

void add_check_ge_u8(TestReport& r, const char* label, uint8_t expect_min, uint8_t actual) {
    char exp[40], act[40];
    snprintf(exp, sizeof(exp), ">=%u", expect_min);
    snprintf(act, sizeof(act), "%u", actual);
    add_check(r, label, actual >= expect_min, exp, act);
}

void add_check_range_i(TestReport& r, const char* label, int minv, int maxv, int actual) {
    char exp[40], act[40];
    snprintf(exp, sizeof(exp), "%d..%d", minv, maxv);
    snprintf(act, sizeof(act), "%d", actual);
    add_check(r, label, (actual >= minv && actual <= maxv), exp, act);
}

void add_check_bool(TestReport& r, const char* label, bool expect, bool actual) {
    add_check(r, label, expect == actual, expect ? "true" : "false", actual ? "true" : "false");
}

bool summarize_checks(TestReport& r) {
    bool ok = true;
    for (uint8_t i = 0; i < r.check_count; ++i) {
        if (!r.checks[i].ok) { ok = false; break; }
    }
    r.success = ok;
    return ok;
}

void print_separator() {
    Serial.println("--------------------------------------------------------------------------------");
}

void print_compact_header() {
    print_separator();
    Serial.printf("%-14s  %-5s  %-7s  %-40s\n", "Test", "Res", "Time", "Note");
    print_separator();
}

void print_compact_row(const TestReport& r) {
    Serial.printf("%-14s  %-5s  %6lums  %-40s\n",
        r.name,
        r.success ? "PASS" : "FAIL",
        (unsigned long)r.duration_ms,
        r.note);
}

void print_checks_verbose(const TestReport& r) {
    Serial.println();
    Serial.println("checks:");
    for (uint8_t i = 0; i < r.check_count; ++i) {
        const AssertionResult& c = r.checks[i];
        Serial.printf("  %-18s expected=%-12s actual=%-12s %s\n",
            c.label,
            c.expected,
            c.actual,
            c.ok ? "PASS" : "FAIL");
    }
    Serial.println();
    Serial.println("before:");
    print_probe_line("", r.before);
    if (r.has_fail_snapshot) {
        Serial.println();
        Serial.println("fail:");
        print_probe_line("", r.fail);
    }
    Serial.println();
    Serial.println("after:");
    print_probe_line("", r.after);
}

bool execute_test(const TestSpec& spec, TestReport& rep) {
    memset(&rep, 0, sizeof(rep));
    rep.name = spec.name;
    capture_probe(rep.before);
    uint32_t start = millis();
    bool action_ok = spec.action ? spec.action(rep) : false;
    capture_probe(rep.after);
    rep.duration_ms = millis() - start;
    add_check_bool(rep, "action_ok", true, action_ok);
    add_check_eq_u8(rep, "i2c.ping", 0xAA, rep.after.i2c_ping);
    add_check_eq_u8(rep, "engine.error", 0x00, rep.after.raw.error_code);
    summarize_checks(rep);
    return rep.success;
}

bool action_begin(TestReport& r) {
    bool ok = display.begin();
    add_check_bool(r, "begin_ret", true, ok);
    snprintf(r.note, sizeof(r.note), "begin()=%s", ok ? "PASS" : "FAIL");
    return ok;
}

bool action_ping(TestReport& r) {
    uint8_t v = display.ping();
    add_check_eq_u8(r, "ping", 0xAA, v);
    snprintf(r.note, sizeof(r.note), "ping=0x%02X", v);
    return v == 0xAA;
}

bool action_version(TestReport& r) {
    uint8_t maj = display.getFirmwareMajorVersion();
    uint8_t min = display.getFirmwareMinorVersion();
    uint8_t pat = display.getFirmwarePatchVersion();
    add_check_eq_u8(r, "fw.major", 0, maj);
    add_check_eq_u8(r, "fw.minor", 2, min);
    add_check_ge_u8(r, "fw.patch", 2, pat);
    snprintf(r.note, sizeof(r.note), "v%u.%u.%u", maj, min, pat);
    return true;
}

bool action_sysinfo(TestReport& r) {
    W32DL1414SystemInfo sys = display.getSystemInfo();
    add_check_eq_u8(r, "board", 2, sys.board);
    add_check_range_i(r, "clock_khz", 7000, 9000, sys.clock_khz);
    add_check_eq_u8(r, "ram_total", 0x00, sys.ram_total);
    add_check_range_i(r, "ram_free_min", 100, 512, sys.ram_free);
    snprintf(r.note, sizeof(r.note), "board=%u clk=%ukHz ram=%u/%u", sys.board, sys.clock_khz, sys.ram_free, sys.ram_total);
    return true;
}

bool action_write_char(TestReport& r) {
    if (!prepare_display_test("write_char")) {
        capture_fail_snapshot(r);
        snprintf(r.note, sizeof(r.note), "prepare failed");
        add_check_bool(r, "prepare", true, false);
        return false;
    }
    bool ok = display.writeChar(0, 'X');

    Serial.printf(
        "writeChar=%s client=%u(%s) wire=%u resp=%u dev=0x%02X\n",
        ok ? "true" : "false",
        display.getLastClientError(),
        w32dl1414_client_error_text(display.getLastClientError()),
        display.getLastWireError(),
        display.getLastResponseLen(),
        display.getLastDeviceError()
    );

    if (!ok) {
        capture_fail_snapshot(r);
        add_check_bool(r, "writeChar", true, false);
        snprintf(r.note, sizeof(r.note), "writeChar failed");
        return false;
    }
    ok = display.flush();
    if (!ok) {
        capture_fail_snapshot(r);
        add_check_bool(r, "flush", true, false);
        snprintf(r.note, sizeof(r.note), "flush failed");
        return false;
    }
    ok = wait_idle(30000);
    if (!ok) {
        capture_fail_snapshot(r);
        add_check_bool(r, "wait_idle", true, false);
        snprintf(r.note, sizeof(r.note), "wait_idle failed");
        return false;
    }
    add_check_bool(r, "write+flush+idle", true, true);
    snprintf(r.note, sizeof(r.note), "pos=0 ch='X' + flush");
    return true;
}

bool action_write_buffer(TestReport& r) {
    if (!prepare_display_test("write_buffer")) {
        capture_fail_snapshot(r);
        snprintf(r.note, sizeof(r.note), "prepare failed");
        add_check_bool(r, "prepare", true, false);
        return false;
    }
    const uint8_t buf[] = {'A','B','C','D','E','F','G','H'};
    bool ok = display.writeBuffer(8, buf, sizeof(buf));
    if (!ok) {
        capture_fail_snapshot(r);
        add_check_bool(r, "writeBuffer", true, false);
        snprintf(r.note, sizeof(r.note), "writeBuffer failed");
        return false;
    }
    ok = display.flush();
    if (!ok) {
        capture_fail_snapshot(r);
        add_check_bool(r, "flush", true, false);
        snprintf(r.note, sizeof(r.note), "flush failed");
        return false;
    }
    ok = wait_idle(30000);
    if (!ok) {
        capture_fail_snapshot(r);
        add_check_bool(r, "wait_idle", true, false);
        snprintf(r.note, sizeof(r.note), "wait_idle failed");
        return false;
    }
    add_check_bool(r, "writebuf+flush+idle", true, true);
    snprintf(r.note, sizeof(r.note), "pos=8 len=%u + flush", (unsigned)sizeof(buf));
    return true;
}

bool action_clear(TestReport& r) {
    if (!wait_idle(30000)) {
        capture_fail_snapshot(r);
        snprintf(r.note, sizeof(r.note), "not idle before clear");
        add_check_bool(r, "idle_before", true, false);
        return false;
    }
    display.clear(' ');
    bool ok = display.flush();
    if (!ok) {
        capture_fail_snapshot(r);
        add_check_bool(r, "flush", true, false);
        snprintf(r.note, sizeof(r.note), "flush failed");
        return false;
    }
    ok = wait_idle(8000);
    if (!ok) {
        capture_fail_snapshot(r);
        add_check_bool(r, "wait_idle", true, false);
        snprintf(r.note, sizeof(r.note), "wait_idle failed");
        return false;
    }
    add_check_bool(r, "clear+flush+idle", true, true);
    snprintf(r.note, sizeof(r.note), "clear(' ') + flush");
    return true;
}

bool action_flush(TestReport& r) {
    if (!prepare_display_test("flush")) {
        capture_fail_snapshot(r);
        snprintf(r.note, sizeof(r.note), "prepare failed");
        add_check_bool(r, "prepare", true, false);
        return false;
    }
    display.print(0, "HELLO WORLD TEST");
    bool ok = display.flush();
    if (!ok) {
        capture_fail_snapshot(r);
        add_check_bool(r, "flush", true, false);
        snprintf(r.note, sizeof(r.note), "flush failed");
        return false;
    }
    ok = wait_idle(30000);
    if (!ok) {
        capture_fail_snapshot(r);
        add_check_bool(r, "wait_idle", true, false);
        snprintf(r.note, sizeof(r.note), "wait_idle failed");
        return false;
    }
    add_check_bool(r, "print+flush+idle", true, true);
    snprintf(r.note, sizeof(r.note), "print+flush HELLO WORLD TEST");
    return true;
}

bool action_cursor(TestReport& r) {
    uint8_t old = display.getCursor();
    display.setCursor(42);
    uint8_t now = display.getCursor();
    add_check_eq_u8(r, "cursor_after", 42, now);
    snprintf(r.note, sizeof(r.note), "cursor %u -> %u", old, now);
    return now == 42;
}

bool action_print(TestReport& r) {
    if (!prepare_display_test("print")) {
        capture_fail_snapshot(r);
        snprintf(r.note, sizeof(r.note), "prepare failed");
        add_check_bool(r, "prepare", true, false);
        return false;
    }
    display.print(0, "PRINT TEST");
    bool ok = display.flush();
    if (!ok) {
        capture_fail_snapshot(r);
        add_check_bool(r, "flush", true, false);
        snprintf(r.note, sizeof(r.note), "flush failed");
        return false;
    }
    ok = wait_idle(30000);
    if (!ok) {
        capture_fail_snapshot(r);
        add_check_bool(r, "wait_idle", true, false);
        snprintf(r.note, sizeof(r.note), "wait_idle failed");
        return false;
    }
    add_check_bool(r, "print+flush+idle", true, true);
    snprintf(r.note, sizeof(r.note), "print(0,\"PRINT TEST\") + flush");
    return true;
}

bool action_printf(TestReport& r) {
    if (!prepare_display_test("printf")) {
        capture_fail_snapshot(r);
        snprintf(r.note, sizeof(r.note), "prepare failed");
        add_check_bool(r, "prepare", true, false);
        return false;
    }
    display.printf(0, "NUM=%d POS=%u", 123, 456);
    bool ok = display.flush();
    if (!ok) {
        capture_fail_snapshot(r);
        add_check_bool(r, "flush", true, false);
        snprintf(r.note, sizeof(r.note), "flush failed");
        return false;
    }
    ok = wait_idle(30000);
    if (!ok) {
        capture_fail_snapshot(r);
        add_check_bool(r, "wait_idle", true, false);
        snprintf(r.note, sizeof(r.note), "wait_idle failed");
        return false;
    }
    add_check_bool(r, "printf+flush+idle", true, true);
    snprintf(r.note, sizeof(r.note), "printf(NUM=123 POS=456) + flush");
    return true;
}

bool action_long_printf(TestReport& r) {
    if (!prepare_display_test("long_printf")) {
        capture_fail_snapshot(r);
        snprintf(r.note, sizeof(r.note), "prepare failed");
        add_check_bool(r, "prepare", true, false);
        return false;
    }
    display.printf(0,
        "LEN=%d MODE=%s TEXT=%s",
        120,
        "PRINTF",
        "^!\"\\/$%&()=?'#*+-.,:;-_<>[]0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ@*-*-*-*-*-*-*@@-*-*-*-*-*-*-*-*-*........");
    bool ok = display.flush();
    if (!ok) {
        capture_fail_snapshot(r);
        add_check_bool(r, "flush", true, false);
        snprintf(r.note, sizeof(r.note), "flush failed");
        return false;
    }
    ok = wait_idle(30000);
    if (!ok) {
        capture_fail_snapshot(r);
        add_check_bool(r, "wait_idle", true, false);
        snprintf(r.note, sizeof(r.note), "wait_idle failed");
        return false;
    }
    add_check_bool(r, "longprintf+flush+idle", true, true);
    snprintf(r.note, sizeof(r.note), "long printf + flush");
    return true;
}

bool action_fill_clear(TestReport& r) {
    if (!prepare_display_test("fill_then_clear")) {
        capture_fail_snapshot(r);
        snprintf(r.note, sizeof(r.note), "prepare failed");
        add_check_bool(r, "prepare", true, false);
        return false;
    }
    display.print(0, "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX");
    bool ok = display.flush();
    if (!ok) {
        capture_fail_snapshot(r);
        add_check_bool(r, "flush", true, false);
        snprintf(r.note, sizeof(r.note), "flush failed");
        return false;
    }
    ok = wait_idle(30000);
    if (!ok) {
        capture_fail_snapshot(r);
        add_check_bool(r, "wait_idle", true, false);
        snprintf(r.note, sizeof(r.note), "wait_idle failed");
        return false;
    }
    delay(1000);
    display.clear(' ');
    ok = wait_idle(30000);
    if (!ok) {
        capture_fail_snapshot(r);
        add_check_bool(r, "wait_idle_after_clear", true, false);
        snprintf(r.note, sizeof(r.note), "clear phase failed");
        return false;
    }
    add_check_bool(r, "fill->flush->clear", true, true);
    snprintf(r.note, sizeof(r.note), "fill->flush->clear");
    return true;
}

bool action_cycle_clear_chars(TestReport& r) {
    if (!prepare_display_test("cycle_clear_chars")) {
        capture_fail_snapshot(r);
        snprintf(r.note, sizeof(r.note), "prepare failed");
        add_check_bool(r, "prepare", true, false);
        return false;
    }

    static const char chars[] = { '1', '/', '-', '\\', '*' };

    while (true) {
        for (uint8_t i = 0; i < sizeof(chars); ++i) {
            if (Serial.available()) {
                while (Serial.available()) {
                    Serial.read();
                }
                add_check_bool(r, "cycle+flush+keypress", true, true);
                snprintf(r.note, sizeof(r.note), "clear/flush cycle stopped by keypress");
                return true;
            }

            display.clear(chars[i]);
            
            bool ok = display.flush();
            if (!ok) {
                capture_fail_snapshot(r);
                add_check_bool(r, "flush", true, false);
                snprintf(r.note, sizeof(r.note), "flush failed after clear('%c')", chars[i]);
                return false;
            }

            ok = wait_idle(30000);
            if (!ok) {
                capture_fail_snapshot(r);
                add_check_bool(r, "wait_idle", true, false);
                snprintf(r.note, sizeof(r.note), "wait_idle failed after clear('%c')", chars[i]);
                return false;
            }
        }
    }
}

bool action_cursor_enable(TestReport& r) {
    if (!prepare_display_test("cursor_enable")) {
        capture_fail_snapshot(r);
        snprintf(r.note, sizeof(r.note), "prepare failed");
        add_check_bool(r, "prepare", true, false);
        return false;
    }

    bool ok = display.setCursorEnable(true);
    if (!ok) {
        capture_fail_snapshot(r);
        add_check_bool(r, "setCursorEnable", true, false);
        snprintf(r.note, sizeof(r.note), "setCursorEnable(true) failed");
        return false;
    }

    bool value = false;
    ok = display.getCursorEnable(&value);
    if (!ok) {
        capture_fail_snapshot(r);
        add_check_bool(r, "getCursorEnable", true, false);
        snprintf(r.note, sizeof(r.note), "getCursorEnable() failed");
        return false;
    }

    add_check_bool(r, "cursor_enable", true, value);
    snprintf(r.note, sizeof(r.note), "set/get cursor enable");
    return value == true;
}

bool action_cursor_visible(TestReport& r) {
    if (!prepare_display_test("cursor_visible")) {
        capture_fail_snapshot(r);
        snprintf(r.note, sizeof(r.note), "prepare failed");
        add_check_bool(r, "prepare", true, false);
        return false;
    }

    bool ok = display.setCursorVisible(true);
    if (!ok) {
        capture_fail_snapshot(r);
        add_check_bool(r, "setCursorVisible", true, false);
        snprintf(r.note, sizeof(r.note), "setCursorVisible(true) failed");
        return false;
    }

    bool value = false;
    ok = display.getCursorVisible(&value);
    if (!ok) {
        capture_fail_snapshot(r);
        add_check_bool(r, "getCursorVisible", true, false);
        snprintf(r.note, sizeof(r.note), "getCursorVisible() failed");
        return false;
    }

    add_check_bool(r, "cursor_visible", true, value);
    snprintf(r.note, sizeof(r.note), "set/get cursor visible");
    return value == true;
}

bool action_cursor_pos(TestReport& r) {
    if (!prepare_display_test("cursor_pos")) {
        capture_fail_snapshot(r);
        snprintf(r.note, sizeof(r.note), "prepare failed");
        add_check_bool(r, "prepare", true, false);
        return false;
    }

    const uint8_t expected = 42;
    bool ok = display.setCursorPos(expected);
    if (!ok) {
        capture_fail_snapshot(r);
        add_check_bool(r, "setCursorPos", true, false);
        snprintf(r.note, sizeof(r.note), "setCursorPos(%u) failed", expected);
        return false;
    }

    uint8_t actual = 0xFF;
    ok = display.getCursorPos(&actual);
    if (!ok) {
        capture_fail_snapshot(r);
        add_check_bool(r, "getCursorPos", true, false);
        snprintf(r.note, sizeof(r.note), "getCursorPos() failed");
        return false;
    }

    add_check_eq_u8(r, "cursor_pos", expected, actual);
    snprintf(r.note, sizeof(r.note), "set/get cursor pos %u", expected);
    return actual == expected;
}

bool action_cursor_char(TestReport& r) {
    if (!prepare_display_test("cursor_char")) {
        capture_fail_snapshot(r);
        snprintf(r.note, sizeof(r.note), "prepare failed");
        add_check_bool(r, "prepare", true, false);
        return false;
    }

    const uint8_t expected = '_';
    bool ok = display.setCursorChar((char)expected);
    if (!ok) {
        capture_fail_snapshot(r);
        add_check_bool(r, "setCursorChar", true, false);
        snprintf(r.note, sizeof(r.note), "setCursorChar('_') failed");
        return false;
    }

    uint8_t actual = 0x00;
    ok = display.getCursorChar((char*)&actual);
    if (!ok) {
        capture_fail_snapshot(r);
        add_check_bool(r, "getCursorChar", true, false);
        snprintf(r.note, sizeof(r.note), "getCursorChar() failed");
        return false;
    }

    add_check_eq_u8(r, "cursor_char", expected, actual);
    snprintf(r.note, sizeof(r.note), "set/get cursor char '_'");
    return actual == expected;
}

bool action_cursor_blink_10ms(TestReport& r) {
    if (!prepare_display_test("cursor_blink_10ms")) {
        capture_fail_snapshot(r);
        snprintf(r.note, sizeof(r.note), "prepare failed");
        add_check_bool(r, "prepare", true, false);
        return false;
    }

    const uint8_t expected = 25;
    bool ok = display.setCursorBlink10ms(expected);
    if (!ok) {
        capture_fail_snapshot(r);
        add_check_bool(r, "setCursorBlink10ms", true, false);
        snprintf(r.note, sizeof(r.note), "setCursorBlink10ms(%u) failed", expected);
        return false;
    }

    uint8_t actual = 0xFF;
    ok = display.getCursorBlink10ms(&actual);
    if (!ok) {
        capture_fail_snapshot(r);
        add_check_bool(r, "getCursorBlink10ms", true, false);
        snprintf(r.note, sizeof(r.note), "getCursorBlink10ms() failed");
        return false;
    }

    add_check_eq_u8(r, "cursor_blink_10ms", expected, actual);
    snprintf(r.note, sizeof(r.note), "set/get cursor blink %u x10ms", expected);
    return actual == expected;
}

bool action_refresh_mode(TestReport& r) {
    if (!prepare_display_test("refresh_mode")) {
        capture_fail_snapshot(r);
        snprintf(r.note, sizeof(r.note), "prepare failed");
        add_check_bool(r, "prepare", true, false);
        return false;
    }

    const uint8_t expected = 1;
    bool ok = display.setRefreshMode(expected);
    if (!ok) {
        capture_fail_snapshot(r);
        add_check_bool(r, "setRefreshMode", true, false);
        snprintf(r.note, sizeof(r.note), "setRefreshMode(%u) failed", expected);
        return false;
    }

    uint8_t actual = 0xFF;
    ok = display.getRefreshMode(&actual);
    if (!ok) {
        capture_fail_snapshot(r);
        add_check_bool(r, "getRefreshMode", true, false);
        snprintf(r.note, sizeof(r.note), "getRefreshMode() failed");
        return false;
    }

    add_check_eq_u8(r, "refresh_mode", expected, actual);
    snprintf(r.note, sizeof(r.note), "set/get refresh mode %u", expected);
    return actual == expected;
}

bool action_scroll_mode(TestReport& r) {
    if (!prepare_display_test("scroll_mode")) {
        capture_fail_snapshot(r);
        snprintf(r.note, sizeof(r.note), "prepare failed");
        add_check_bool(r, "prepare", true, false);
        return false;
    }

    const uint8_t expected = 1;
    bool ok = display.setScrollMode(expected);
    if (!ok) {
        capture_fail_snapshot(r);
        add_check_bool(r, "setScrollMode", true, false);
        snprintf(r.note, sizeof(r.note), "setScrollMode(%u) failed", expected);
        return false;
    }

    uint8_t actual = 0xFF;
    ok = display.getScrollMode(&actual);
    if (!ok) {
        capture_fail_snapshot(r);
        add_check_bool(r, "getScrollMode", true, false);
        snprintf(r.note, sizeof(r.note), "getScrollMode() failed");
        return false;
    }

    add_check_eq_u8(r, "scroll_mode", expected, actual);
    snprintf(r.note, sizeof(r.note), "set/get scroll mode %u", expected);
    return actual == expected;
}

bool action_read_dip(TestReport& r) {
    uint8_t dip = 0xFF;
    bool ok = display.readDip(&dip);
    if (!ok) {
        capture_fail_snapshot(r);
        add_check_bool(r, "readDip", true, false);
        snprintf(r.note, sizeof(r.note), "readDip() failed");
        return false;
    }

    add_check_range_i(r, "dip", 0, 255, dip);
    snprintf(r.note, sizeof(r.note), "dip=0x%02X", dip);
    return true;
}

bool action_soft_reset(TestReport& r) {
    bool ok = display.softReset();
    if (!ok) {
        capture_fail_snapshot(r);
        add_check_bool(r, "softReset", true, false);
        snprintf(r.note, sizeof(r.note), "softReset() failed");
        return false;
    }

    delay(50);
    uint8_t ping = display.ping();
    add_check_eq_u8(r, "ping_after_reset", 0xAA, ping);
    snprintf(r.note, sizeof(r.note), "soft reset");
    return ping == 0xAA;
}

bool action_hard_reset(TestReport& r) {
    bool ok = display.hardReset();
    if (!ok) {
        capture_fail_snapshot(r);
        add_check_bool(r, "hardReset", true, false);
        snprintf(r.note, sizeof(r.note), "hardReset() failed");
        return false;
    }

    delay(100);
    bool begin_ok = display.begin();
    add_check_bool(r, "begin_after_reset", true, begin_ok);
    if (!begin_ok) {
        capture_fail_snapshot(r);
        snprintf(r.note, sizeof(r.note), "begin after hard reset failed");
        return false;
    }

    uint8_t ping = display.ping();
    add_check_eq_u8(r, "ping_after_reset", 0xAA, ping);
    snprintf(r.note, sizeof(r.note), "hard reset + begin");
    return ping == 0xAA;
}

bool action_status(TestReport& r) {
    W32DL1414RawStatus st = display.getStatusRaw();
    char buf[128];
    snprintf(buf, sizeof(buf), "eng=%u op=%u err=%u job=%u type=%u off=%u prog=%u",
        st.engine_status, st.operation_status, st.error_code, st.job_status, st.job_type, st.job_offset, st.job_progress);
    add_check_bool(r, "status_read", true, true);
    snprintf(r.note, sizeof(r.note), "%s", buf);
    return true;
}

TestSpec tests[] = {
    {'1', "begin",       true,  action_begin},
    {'2', "ping",        true,  action_ping},
    {'3', "version",     true,  action_version},
    {'4', "sysinfo",     true,  action_sysinfo},
    {'5', "write_char",  false, action_write_char},
    {'6', "write_buffer",false, action_write_buffer},
    {'7', "clear",       false, action_clear},
    {'8', "flush",       false, action_flush},
    {'9', "cursor",      true,  action_cursor},
    {'p', "print",       false, action_print},
    {'f', "printf",      false, action_printf},
    {'l', "long_printf", false, action_long_printf},
    {'k', "fill_clear",  false, action_fill_clear},
    {'s', "status",      true,  action_status},
    {'t', "cycle_clear_chars", false, action_cycle_clear_chars},
    {'u', "cursor_enable",       true,  action_cursor_enable},
    {'v', "cursor_visible",      true,  action_cursor_visible},
    {'m', "cursor_pos",          true,  action_cursor_pos},
    {'n', "cursor_char",         true,  action_cursor_char},
    {'b', "cursor_blink_10ms",   true,  action_cursor_blink_10ms},
    {'r', "refresh_mode",        true,  action_refresh_mode},
    {'o', "scroll_mode",         true,  action_scroll_mode},
    {'d', "read_dip",            true,  action_read_dip},
    {'y', "soft_reset",          false, action_soft_reset},
    {'x', "hard_reset",          false, action_hard_reset}
};

const TestSpec* find_test_by_key(char k) {
    for (size_t i = 0; i < sizeof(tests) / sizeof(tests[0]); ++i) {
        if (tests[i].key == k) return &tests[i];
    }
    return nullptr;
}

void run_named_test_by_spec(const TestSpec& t) {
    TestReport r;
    execute_test(t, r);
    Serial.println();
    print_compact_header();
    print_compact_row(r);
    print_separator();
    print_checks_verbose(r);
}

void run_all_tests() {
    Serial.println();
    Serial.println("Running full test suite...");
    for (size_t i = 0; i < sizeof(tests) / sizeof(tests[0]); ++i) {
        run_named_test_by_spec(tests[i]);
    }
}

void print_help() 
{
    Serial.println();
    Serial.println("======================================================");
    Serial.println(" W32DL1414 TEST & DEBUG CONSOLE");
    Serial.println("======================================================");

    Serial.println(" General:");
    Serial.println("   h  help");
    Serial.println("   a  run all tests");
    Serial.println("   s  show raw status");
    Serial.println();

    Serial.println(" Basic:");
    Serial.println("   1  begin");
    Serial.println("   2  ping");
    Serial.println("   3  version");
    Serial.println("   4  sysinfo");
    Serial.println("   9  cursor set/get");
    Serial.println();

    Serial.println(" Text / buffer:");
    Serial.println("   5  write_char + flush");
    Serial.println("   6  write_buffer + flush");
    Serial.println("   7  clear + flush");
    Serial.println("   8  print + flush (HELLO WORLD TEST)");
    Serial.println("   p  print + flush (PRINT TEST)");
    Serial.println("   f  printf + flush");
    Serial.println("   l  long printf + flush");
    Serial.println("   k  fill + flush + clear");
    Serial.println("   t  cycle clear chars");
    Serial.println();

    Serial.println(" Cursor:");
    Serial.println("   u  cursor enable set/get");
    Serial.println("   v  cursor visible set/get");
    Serial.println("   m  cursor pos set/get");
    Serial.println("   n  cursor char set/get");
    Serial.println("   b  cursor blink x10ms set/get");
    Serial.println();

    Serial.println(" Modes:");
    Serial.println("   r  refresh mode set/get");
    Serial.println("   o  scroll mode set/get");
    Serial.println();

    Serial.println(" System:");
    Serial.println("   d  read dip");
    Serial.println("   y  soft reset");
    Serial.println("   x  hard reset");
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
                    run_named_test_by_spec(tests[i]);
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
    Serial.println("Booting W32DL1414 debug console (refactored)...");
    bool ok = display.begin();
    Serial.printf("Display begin: %s\n", ok ? "PASS" : "FAIL");
    print_status_block("INITIAL STATUS", display.getStatusRaw());
    print_help();
}

void loop() {
    handle_serial();
}
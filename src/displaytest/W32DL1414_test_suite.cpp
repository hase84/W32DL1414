#include <Arduino.h>
#include <Wire.h>
#include <cstring>
#include <cstdio>

#include "displayclass/W32DL1414.h"
#include "shared/W32DL1414_prtcl.h"

#define USE_PLAIN_ASCII 1

namespace
{
constexpr uint8_t kDisplayAddress = 0x40;
constexpr uint8_t kCheckCapacity = 20;
constexpr uint16_t kIdleTimeoutMs = 8000;
constexpr uint16_t kLongIdleTimeoutMs = 30000;
constexpr uint16_t kIdleSettleMs = 20;

W32DL1414 display(kDisplayAddress);

struct ProbeSnapshot {
    bool i2c_ok = false;
    uint8_t i2c_ping = 0;
    W32DL1414SystemInfo sys = {};
    W32DL1414RawStatus raw = {};
};

struct AssertionResult {
    const char* label = nullptr;
    bool ok = false;
    char expected[48] = {0};
    char actual[48] = {0};
};

struct TestReport {
    const char* name = nullptr;
    bool success = false;
    uint32_t duration_ms = 0;
    ProbeSnapshot before = {};
    ProbeSnapshot fail = {};
    ProbeSnapshot after = {};
    bool has_fail_snapshot = false;
    AssertionResult checks[kCheckCapacity] = {};
    uint8_t check_count = 0;
    char note[128] = {0};
};

using TestActionFn = bool (*)(TestReport&);

struct TestSpec {
    char key;
    const char* name;
    bool reads_only;
    TestActionFn action;
};

struct OpcodeInfo {
    uint8_t opcode;
    const char* name;
    bool allowed_during_job;
    uint8_t min_input_len;
    uint8_t max_input_len;
};

struct JobTypeInfo {
    uint8_t job_type;
    const char* name;
    uint8_t steps_per_tick;
    uint8_t settle_ticks;
};

static const OpcodeInfo kOpcodeTable[] = {
    { W32DL1414_OPCODE_PING,                "PING",                true,  1, 1 },
    { W32DL1414_OPCODE_GET_VERSION,         "GET_VERSION",         true,  1, 1 },
    { W32DL1414_OPCODE_GET_STATUS,          "GET_STATUS",          true,  1, 1 },
    { W32DL1414_OPCODE_GET_SYSTEM_INFO,     "GET_SYSTEM_INFO",     true,  1, 1 },

    { W32DL1414_OPCODE_WRITE_CHAR,          "WRITE_CHAR",          false, 3, 3 },
    { W32DL1414_OPCODE_WRITE_BUF,           "WRITE_BUF",           false, 4, W32DL1414_MAX_TRANSFER_LEN },
    { W32DL1414_OPCODE_READ_BUF,            "READ_BUF",            false, 3, 3 },
    { W32DL1414_OPCODE_FLUSH,               "FLUSH",               false, 1, 1 },
    { W32DL1414_OPCODE_CLEAR,               "CLEAR",               false, 2, 2 },

    { W32DL1414_OPCODE_SET_DISPLAY_CONFIG,  "SET_DISPLAY_CONFIG",  false, 4, 4 },
    { W32DL1414_OPCODE_GET_DISPLAY_CONFIG,  "GET_DISPLAY_CONFIG",  false, 1, 1 },
    { W32DL1414_OPCODE_SET_CURSOR_CONFIG,   "SET_CURSOR_CONFIG",   false, 8, 8 },
    { W32DL1414_OPCODE_GET_CURSOR_CONFIG,   "GET_CURSOR_CONFIG",   false, 1, 1 },

    { W32DL1414_OPCODE_SOFT_RESET,          "SOFT_RESET",          true,  1, 1 },
    { W32DL1414_OPCODE_HARD_RESET,          "HARD_RESET",          true,  1, 1 },
    { W32DL1414_OPCODE_READ_DIP,            "READ_DIP",            true,  1, 1 },
};

static const JobTypeInfo kJobTypeTable[] = {
    { W32DL1414_JOB_TYPE_FLUSH_VRAM,  "FLUSH_VRAM",  4, 1 },
    { W32DL1414_JOB_TYPE_CLEAR_VRAM,  "CLEAR_VRAM",  4, 0 },
    { W32DL1414_JOB_TYPE_SCROLL_VRAM, "SCROLL_VRAM", 4, 0 },
};

const OpcodeInfo* find_opcode_info(uint8_t opcode)
{
    for (size_t i = 0; i < sizeof(kOpcodeTable) / sizeof(kOpcodeTable[0]); ++i) {
        if (kOpcodeTable[i].opcode == opcode) {
            return &kOpcodeTable[i];
        }
    }
    return nullptr;
}

const JobTypeInfo* find_job_type_info(uint8_t job_type)
{
    for (size_t i = 0; i < sizeof(kJobTypeTable) / sizeof(kJobTypeTable[0]); ++i) {
        if (kJobTypeTable[i].job_type == job_type) {
            return &kJobTypeTable[i];
        }
    }
    return nullptr;
}

const char* bool_text(bool v)
{
    return v ? "true" : "false";
}

const char* opcode_text(uint8_t opcode)
{
    const OpcodeInfo* info = find_opcode_info(opcode);
    return info ? info->name : "?";
}

const char* result_text(uint8_t v)
{
    switch (v) {
        case W32DL1414_RESULT_OK: return "RESULT_OK";
        case W32DL1414_RESULT_FAILED: return "RESULT_FAILED";
        default: return "?";
    }
}

const char* engine_status_text(uint8_t v)
{
    switch (v) {
        case W32DL1414_ENGINE_STATUS_READY: return "READY";
        case W32DL1414_ENGINE_STATUS_BUSY: return "BUSY";
        case W32DL1414_ENGINE_STATUS_ERROR: return "ERROR";
        default: return "?";
    }
}

const char* operation_status_text(uint8_t v)
{
    switch (v) {
        case W32DL1414_OPERATION_STATUS_IDLE: return "IDLE";
        case W32DL1414_OPERATION_STATUS_RUNNING: return "RUNNING";
        case W32DL1414_OPERATION_STATUS_OK: return "OK";
        case W32DL1414_OPERATION_STATUS_FAILED: return "FAILED";
        default: return "?";
    }
}

const char* error_text(uint8_t v)
{
    switch (v) {
        case W32DL1414_ERROR_NONE: return "NONE";
        case W32DL1414_ERROR_INVALID_OPCODE: return "INVALID_OPCODE";
        case W32DL1414_ERROR_INVALID_ARGUMENT: return "INVALID_ARGUMENT";
        case W32DL1414_ERROR_OUT_OF_RANGE: return "OUT_OF_RANGE";
        case W32DL1414_ERROR_BUSY: return "BUSY";
        case W32DL1414_ERROR_INTERNAL: return "INTERNAL";
        case W32DL1414_ERROR_BUFFER_OVERFLOW: return "BUFFER_OVERFLOW";
        case W32DL1414_ERROR_NOT_IMPLEMENTED: return "NOT_IMPLEMENTED";
        case W32DL1414_ERROR_INVALID_LENGTH: return "INVALID_LENGTH";
        default: return "?";
    }
}

const char* job_status_text(uint8_t v)
{
    switch (v) {
        case W32DL1414_JOB_STATUS_NONE: return "NONE";
        case W32DL1414_JOB_STATUS_PENDING: return "PENDING";
        case W32DL1414_JOB_STATUS_RUNNING: return "RUNNING";
        case W32DL1414_JOB_STATUS_DONE: return "DONE";
        case W32DL1414_JOB_STATUS_FAILED: return "FAILED";
        default: return "?";
    }
}

const char* job_type_text(uint8_t job_type)
{
    if (job_type == W32DL1414_JOB_TYPE_NONE) {
        return "NONE";
    }

    const JobTypeInfo* info = find_job_type_info(job_type);
    return info ? info->name : "?";
}

const char* board_text(uint8_t v)
{
    switch (v) {
        case W32DL1414_BOARD_ID_UNKNOWN: return "UNKNOWN";
        case W32DL1414_BOARD_ID_ATTINY44: return "ATTINY44";
        case W32DL1414_BOARD_ID_ATTINY84: return "ATTINY84";
        default: return "?";
    }
}

const char* refresh_mode_text(uint8_t v)
{
    switch (v) {
        case W32DL1414_REFRESH_MODE_AUTO: return "AUTO";
        case W32DL1414_REFRESH_MODE_MANUAL: return "MANUAL";
        default: return "?";
    }
}

const char* insert_mode_text(uint8_t v)
{
    switch (v) {
        case W32DL1414_INSERT_MODE_TRUNCATE: return "TRUNCATE";
        case W32DL1414_INSERT_MODE_SCROLL_LEFT: return "SCROLL_LEFT";
        case W32DL1414_INSERT_MODE_SCROLL_UP: return "SCROLL_UP";
        case W32DL1414_INSERT_MODE_FLIP_OVER: return "FLIP_OVER";
        default: return "?";
    }
}

void append_flag_text(char* out, size_t out_size, bool& first, const char* text)
{
    const size_t len = strlen(out);
    if (len >= out_size - 1) {
        return;
    }
    snprintf(out + len, out_size - len, "%s%s", first ? "" : "|", text);
    first = false;
}

void format_write_flags(char* out, size_t out_size, uint8_t flags)
{
    out[0] = '\0';

    if (flags == W32DL1414_WRITE_FLAG_NONE) {
        snprintf(out, out_size, "NONE");
        return;
    }

    bool first = true;
    if (flags & W32DL1414_WRITE_FLAG_TRUNCATED) {
        append_flag_text(out, out_size, first, "TRUNCATED");
    }
    if (flags & W32DL1414_WRITE_FLAG_SCROLLED) {
        append_flag_text(out, out_size, first, "SCROLLED");
    }
}

void format_state(char* out, size_t out_size, const W32DL1414RawStatus& st)
{
    const JobTypeInfo* job_info = find_job_type_info(st.job_type);

    if (job_info != nullptr) {
        snprintf(out,
                 out_size,
                 "engine=%s op=%s error=%s job=%s type=%s steps=%u settle=%u offset=%u progress=%u%%",
                 engine_status_text(st.engine_status),
                 operation_status_text(st.operation_status),
                 error_text(st.error_code),
                 job_status_text(st.job_status),
                 job_info->name,
                 job_info->steps_per_tick,
                 job_info->settle_ticks,
                 st.job_offset,
                 st.job_progress);
    } else {
        snprintf(out,
                 out_size,
                 "engine=%s op=%s error=%s job=%s type=%s offset=%u progress=%u%%",
                 engine_status_text(st.engine_status),
                 operation_status_text(st.operation_status),
                 error_text(st.error_code),
                 job_status_text(st.job_status),
                 job_type_text(st.job_type),
                 st.job_offset,
                 st.job_progress);
    }
}

void print_probe_line(const char* prefix, const ProbeSnapshot& p)
{
    Serial.printf("  %si2c     ping=0x%02X ok=%s\n", prefix, p.i2c_ping, bool_text(p.i2c_ok));
    Serial.printf("  %ssystem  board=%u(%s) clk=%ukHz ram=%u/%u\n",
                  prefix,
                  p.sys.board,
                  board_text(p.sys.board),
                  p.sys.clock_khz,
                  p.sys.ram_free,
                  p.sys.ram_total);

    const JobTypeInfo* job_info = find_job_type_info(p.raw.job_type);
    if (job_info != nullptr) {
        Serial.printf("  %sengine  engine=%s op=%s error=%s job=%s type=%u(%s) steps=%u settle=%u offset=%u progress=%u%%\n",
                      prefix,
                      engine_status_text(p.raw.engine_status),
                      operation_status_text(p.raw.operation_status),
                      error_text(p.raw.error_code),
                      job_status_text(p.raw.job_status),
                      p.raw.job_type,
                      job_info->name,
                      job_info->steps_per_tick,
                      job_info->settle_ticks,
                      p.raw.job_offset,
                      p.raw.job_progress);
    } else {
        Serial.printf("  %sengine  engine=%s op=%s error=%s job=%s type=%u(%s) offset=%u progress=%u%%\n",
                      prefix,
                      engine_status_text(p.raw.engine_status),
                      operation_status_text(p.raw.operation_status),
                      error_text(p.raw.error_code),
                      job_status_text(p.raw.job_status),
                      p.raw.job_type,
                      job_type_text(p.raw.job_type),
                      p.raw.job_offset,
                      p.raw.job_progress);
    }
}

void print_status_block(const char* title, const W32DL1414RawStatus& st)
{
    char line[160];
    format_state(line, sizeof(line), st);

    Serial.println();
    Serial.printf("=== %s ===\n", title);
    Serial.println(line);
    Serial.printf("raw       job_type=0x%02X op_status=0x%02X err=0x%02X\n",
                  st.job_type,
                  st.operation_status,
                  st.error_code);
}

bool wait_idle(uint16_t timeout_ms = kIdleTimeoutMs)
{
    return display.waitUntilIdle(timeout_ms);
}

bool wait_stable_idle(uint16_t timeout_ms = kIdleTimeoutMs, uint16_t settle_ms = kIdleSettleMs)
{
    if (!display.waitUntilIdle(timeout_ms)) {
        return false;
    }

    delay(settle_ms);

    const W32DL1414RawStatus st = display.getStatusRaw();
    return (st.job_status == W32DL1414_JOB_STATUS_DONE || st.job_status == W32DL1414_JOB_STATUS_NONE)
        && st.operation_status == W32DL1414_OPERATION_STATUS_OK
        && st.error_code == W32DL1414_ERROR_NONE;
}

bool prepare_display_test()
{
    return wait_stable_idle(kLongIdleTimeoutMs, kIdleSettleMs);
}

bool capture_probe(ProbeSnapshot& p)
{
    p.i2c_ping = display.ping();
    p.i2c_ok = (p.i2c_ping == 0xAA);
    p.sys = display.getSystemInfo();
    p.raw = display.getStatusRaw();
    return true;
}

void capture_fail_snapshot(TestReport& r)
{
    if (!r.has_fail_snapshot) {
        capture_probe(r.fail);
        r.has_fail_snapshot = true;
    }
}

void add_check(TestReport& r, const char* label, bool ok, const char* expected, const char* actual)
{
    if (r.check_count >= kCheckCapacity) {
        return;
    }

    AssertionResult& c = r.checks[r.check_count++];
    c.label = label;
    c.ok = ok;

    strncpy(c.expected, expected, sizeof(c.expected) - 1);
    c.expected[sizeof(c.expected) - 1] = '\0';

    strncpy(c.actual, actual, sizeof(c.actual) - 1);
    c.actual[sizeof(c.actual) - 1] = '\0';
}

void add_check_eq_u8(TestReport& r, const char* label, uint8_t expect, uint8_t actual)
{
    char exp[48];
    char act[48];
    snprintf(exp, sizeof(exp), "0x%02X", expect);
    snprintf(act, sizeof(act), "0x%02X", actual);
    add_check(r, label, expect == actual, exp, act);
}

void add_check_ge_u8(TestReport& r, const char* label, uint8_t expect_min, uint8_t actual)
{
    char exp[48];
    char act[48];
    snprintf(exp, sizeof(exp), ">=%u", expect_min);
    snprintf(act, sizeof(act), "%u", actual);
    add_check(r, label, actual >= expect_min, exp, act);
}

void add_check_range_i(TestReport& r, const char* label, int minv, int maxv, int actual)
{
    char exp[48];
    char act[48];
    snprintf(exp, sizeof(exp), "%d..%d", minv, maxv);
    snprintf(act, sizeof(act), "%d", actual);
    add_check(r, label, actual >= minv && actual <= maxv, exp, act);
}

void add_check_bool(TestReport& r, const char* label, bool expect, bool actual)
{
    add_check(r,
              label,
              expect == actual,
              expect ? "true" : "false",
              actual ? "true" : "false");
}

bool summarize_checks(TestReport& r)
{
    bool ok = true;
    for (uint8_t i = 0; i < r.check_count; ++i) {
        if (!r.checks[i].ok) {
            ok = false;
            break;
        }
    }
    r.success = ok;
    return ok;
}

bool fail_step(TestReport& r, const char* check_label, const char* note)
{
    capture_fail_snapshot(r);
    add_check_bool(r, check_label, true, false);
    snprintf(r.note, sizeof(r.note), "%s", note);
    return false;
}

bool flush_and_wait(TestReport& r, const char* success_note, const char* check_label)
{
    if (!display.flush()) {
        return fail_step(r, "flush", "flush failed");
    }

    if (!wait_idle(kLongIdleTimeoutMs)) {
        return fail_step(r, "wait_idle", "wait_idle failed");
    }

    add_check_bool(r, check_label, true, true);
    snprintf(r.note, sizeof(r.note), "%s", success_note);
    return true;
}

void print_separator()
{
    Serial.println("--------------------------------------------------------------------------------");
}

void print_compact_header()
{
    print_separator();
    Serial.printf("%-18s %-5s %-7s %-48s\n", "Test", "Res", "Time", "Note");
    print_separator();
}

void print_compact_row(const TestReport& r)
{
    Serial.printf("%-18s %-5s %6lums %-48s\n",
                  r.name,
                  r.success ? "PASS" : "FAIL",
                  static_cast<unsigned long>(r.duration_ms),
                  r.note);
}

void print_checks_verbose(const TestReport& r)
{
    Serial.println();
    Serial.println("checks:");
    for (uint8_t i = 0; i < r.check_count; ++i) {
        const AssertionResult& c = r.checks[i];
        Serial.printf("  %-20s expected=%-14s actual=%-14s %s\n",
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

void print_hex_u8(uint8_t value)
{
    if (value < 0x10) {
        Serial.print('0');
    }
    Serial.print(value, HEX);
}

void print_response_raw(const uint8_t* buf, uint8_t len)
{
    Serial.print(F("response  raw=["));
    for (uint8_t i = 0; i < len; ++i) {
        if (i > 0) {
            Serial.print(' ');
        }
        print_hex_u8(buf[i]);
    }
    Serial.println(F("]"));
}

void print_response_mutation(const uint8_t* buf, uint8_t len)
{
    if (len < 5) {
        Serial.print(F("mutation  invalid-len="));
        Serial.println(len);
        return;
    }

    const uint8_t requested = buf[1];
    const uint8_t processed = buf[2];
    const uint8_t changed = buf[3];
    const uint8_t flags = buf[4];

    char flags_text[48];
    format_write_flags(flags_text, sizeof(flags_text), flags);

    Serial.print(F("mutation  requested="));
    Serial.print(requested);
    Serial.print(F(" processed="));
    Serial.print(processed);
    Serial.print(F(" changed="));
    Serial.print(changed);
    Serial.print(F(" flags=0x"));
    print_hex_u8(flags);
    Serial.print(F(" ("));
    Serial.print(flags_text);
    Serial.println(F(")"));

    Serial.print(F("flags     truncated="));
    Serial.print((flags & W32DL1414_WRITE_FLAG_TRUNCATED) ? F("true") : F("false"));
    Serial.print(F(" scrolled="));
    Serial.println((flags & W32DL1414_WRITE_FLAG_SCROLLED) ? F("true") : F("false"));
}

void print_response_data(const uint8_t* buf, uint8_t len)
{
    if (len < 2) {
        Serial.print(F("data      invalid-len="));
        Serial.println(len);
        return;
    }

    const uint8_t count = buf[1];

    Serial.print(F("data      count="));
    Serial.print(count);
    Serial.print(F(" payload=["));

    for (uint8_t i = 0; i < count && static_cast<uint8_t>(i + 2) < len; ++i) {
        if (i > 0) {
            Serial.print(' ');
        }
        print_hex_u8(buf[i + 2]);
    }

    Serial.println(F("]"));
}

void print_response_config(const uint8_t* buf, uint8_t len)
{
    if (len < 3) {
        Serial.print(F("config    invalid-len="));
        Serial.println(len);
        return;
    }

    const uint8_t count = buf[1];
    const uint8_t mask = buf[2];

    Serial.print(F("config    count="));
    Serial.print(count);
    Serial.print(F(" mask=0x"));
    print_hex_u8(mask);
    Serial.print(F(" values=["));

    for (uint8_t i = 0; static_cast<uint8_t>(i + 3) < len; ++i) {
        if (i > 0) {
            Serial.print(' ');
        }
        print_hex_u8(buf[i + 3]);
    }

    Serial.println(F("]"));
}

void print_opcode_details(uint8_t opcode)
{
    const OpcodeInfo* info = find_opcode_info(opcode);
    if (info == nullptr) {
        Serial.printf("opcode    0x%02X (?)\n", opcode);
        return;
    }

    Serial.printf("opcode    0x%02X (%s) allowed_during_job=%s in_len=%u..%u\n",
                  info->opcode,
                  info->name,
                  bool_text(info->allowed_during_job),
                  info->min_input_len,
                  info->max_input_len);
}

void print_last_response_debug_for_opcode(uint8_t opcode)
{
    const uint8_t client = display.getLastClientError();
    const uint8_t wire = display.getLastWireError();
    const uint8_t resp_len = display.getLastResponseLen();
    const uint8_t dev = display.getLastDeviceError();
    const uint8_t* resp_buf = display.getLastResponseBuf();

    print_opcode_details(opcode);

    Serial.printf("client    %u(%s) wire=%u resp=%u dev=0x%02X(%s)\n",
                  client,
                  w32dl1414_client_error_text(client),
                  wire,
                  resp_len,
                  dev,
                  error_text(dev));

    if (resp_len == 0 || resp_buf == nullptr) {
        Serial.println(F("response  <empty>"));
        return;
    }

    Serial.printf("result    0x%02X(%s)\n", resp_buf[0], result_text(resp_buf[0]));
    print_response_raw(resp_buf, resp_len);

    if (opcode == W32DL1414_OPCODE_WRITE_CHAR && resp_buf[0] == W32DL1414_RESULT_OK && resp_len >= 5) {
        print_response_mutation(resp_buf, resp_len);
    } else if ((opcode == W32DL1414_OPCODE_GET_DISPLAY_CONFIG || opcode == W32DL1414_OPCODE_GET_CURSOR_CONFIG)
               && resp_buf[0] == W32DL1414_RESULT_OK) {
        print_response_config(resp_buf, resp_len);
    } else if (resp_buf[0] == W32DL1414_RESULT_OK && resp_len >= 2) {
        print_response_data(resp_buf, resp_len);
    } else if (resp_buf[0] == W32DL1414_RESULT_FAILED && resp_len >= 2) {
        Serial.printf("device    error=0x%02X(%s)\n", resp_buf[1], error_text(resp_buf[1]));
    }
}

bool execute_test(const TestSpec& spec, TestReport& rep)
{
    rep = TestReport{};
    rep.name = spec.name;

    capture_probe(rep.before);

    const uint32_t start = millis();
    const bool action_ok = spec.action ? spec.action(rep) : false;
    rep.duration_ms = millis() - start;

    capture_probe(rep.after);

    add_check_bool(rep, "action_ok", true, action_ok);
    add_check_eq_u8(rep, "i2c.ping", 0xAA, rep.after.i2c_ping);
    add_check_eq_u8(rep, "engine.error", 0x00, rep.after.raw.error_code);

    summarize_checks(rep);
    return rep.success;
}

bool action_begin(TestReport& r)
{
    const bool ok = display.begin();
    add_check_bool(r, "begin_ret", true, ok);
    snprintf(r.note, sizeof(r.note), "begin()=%s", ok ? "PASS" : "FAIL");
    return ok;
}

bool action_ping(TestReport& r)
{
    const uint8_t v = display.ping();
    add_check_eq_u8(r, "ping", 0xAA, v);
    snprintf(r.note, sizeof(r.note), "ping=0x%02X", v);
    return v == 0xAA;
}

bool action_version(TestReport& r)
{
    const uint8_t maj = display.getFirmwareMajorVersion();
    const uint8_t min = display.getFirmwareMinorVersion();
    const uint8_t pat = display.getFirmwarePatchVersion();

    add_check_eq_u8(r, "fw.major", 0, maj);
    add_check_ge_u8(r, "fw.minor", 1, min);
    add_check_ge_u8(r, "fw.patch", 0, pat);

    snprintf(r.note, sizeof(r.note), "v%u.%u.%u", maj, min, pat);
    return true;
}

bool action_sysinfo(TestReport& r)
{
    const W32DL1414SystemInfo sys = display.getSystemInfo();

    add_check_range_i(r, "board", 0, 255, sys.board);
    add_check_range_i(r, "clock_khz", 1000, 20000, sys.clock_khz);
    add_check_range_i(r, "ram_total", 0, 65535, sys.ram_total);
    add_check_range_i(r, "ram_free", 0, 65535, sys.ram_free);

    snprintf(r.note, sizeof(r.note), "board=%u(%s) clk=%ukHz ram=%u/%u",
             sys.board, board_text(sys.board), sys.clock_khz, sys.ram_free, sys.ram_total);
    return true;
}

bool action_write_char(TestReport& r)
{
    if (!prepare_display_test()) {
        return fail_step(r, "prepare", "prepare failed");
    }

    const bool ok = display.writeChar(0, 'X');

    Serial.printf("writeChar=%s\n", ok ? "true" : "false");
    print_last_response_debug_for_opcode(W32DL1414_OPCODE_WRITE_CHAR);

    if (!ok) {
        return fail_step(r, "writeChar", "writeChar failed");
    }

    const W32DL1414RawStatus st = display.getStatusRaw();
    snprintf(r.note,
             sizeof(r.note),
             "op=%s job=%s type=%s",
             opcode_text(W32DL1414_OPCODE_WRITE_CHAR),
             job_status_text(st.job_status),
             job_type_text(st.job_type));

    return flush_and_wait(r, "pos=0 ch='X' + flush", "write+flush+idle");
}

bool action_write_buffer(TestReport& r)
{
    if (!prepare_display_test()) {
        return fail_step(r, "prepare", "prepare failed");
    }

    const uint8_t buf[] = {'A','B','C','D','E','F','G','H'};
    const bool ok = display.writeBuffer(8, buf, sizeof(buf));

    Serial.printf("writeBuffer=%s\n", ok ? "true" : "false");
    print_last_response_debug_for_opcode(W32DL1414_OPCODE_WRITE_BUF);

    if (!ok) {
        return fail_step(r, "writeBuffer", "writeBuffer failed");
    }

    return flush_and_wait(r, "pos=8 len=8 + flush", "writebuf+flush+idle");
}

bool action_clear(TestReport& r)
{
    if (!wait_idle(kLongIdleTimeoutMs)) {
        return fail_step(r, "idle_before", "not idle before clear");
    }

    display.clear(' ');
    print_last_response_debug_for_opcode(W32DL1414_OPCODE_CLEAR);
    return flush_and_wait(r, "clear(' ') + flush", "clear+flush+idle");
}

bool action_flush(TestReport& r)
{
    if (!prepare_display_test()) {
        return fail_step(r, "prepare", "prepare failed");
    }

    display.print(0, "HELLO WORLD TEST");
    const bool flush_ok = display.flush();
    print_last_response_debug_for_opcode(W32DL1414_OPCODE_FLUSH);

    if (!flush_ok) {
        return fail_step(r, "flush", "flush failed");
    }

    if (!wait_idle(kLongIdleTimeoutMs)) {
        return fail_step(r, "wait_idle", "wait_idle failed");
    }

    add_check_bool(r, "print+flush+idle", true, true);
    snprintf(r.note, sizeof(r.note), "print+flush HELLO WORLD TEST");
    return true;
}

bool action_cursor(TestReport& r)
{
    const uint8_t old_pos = display.getCursor();
    display.setCursor(42);
    const uint8_t now = display.getCursor();

    add_check_eq_u8(r, "cursor_after", 42, now);
    snprintf(r.note, sizeof(r.note), "cursor %u -> %u", old_pos, now);
    return now == 42;
}

bool action_print(TestReport& r)
{
    if (!prepare_display_test()) {
        return fail_step(r, "prepare", "prepare failed");
    }

    display.print(0, "PRINT TEST");
    return flush_and_wait(r, "print(0,\"PRINT TEST\") + flush", "print+flush+idle");
}

bool action_printf(TestReport& r)
{
    if (!prepare_display_test()) {
        return fail_step(r, "prepare", "prepare failed");
    }

    display.printf(0, "NUM=%d POS=%u", 123, 456);
    return flush_and_wait(r, "printf(NUM=123 POS=456) + flush", "printf+flush+idle");
}

bool action_long_printf(TestReport& r)
{
    if (!prepare_display_test()) {
        return fail_step(r, "prepare", "prepare failed");
    }

    display.printf(
        0,
        "LEN=%d MODE=%s TEXT=%s",
        120,
        "PRINTF",
        "^!\"\\\\/$%&()=?'#*+-.,:;-_<>[]0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ@*-*-*-*-*-*-*@@-*-*-*-*-*-*-*-*-*........");

    return flush_and_wait(r, "long printf + flush", "longprintf+flush+idle");
}

bool action_fill_clear(TestReport& r)
{
    if (!prepare_display_test()) {
        return fail_step(r, "prepare", "prepare failed");
    }

    display.print(0, "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX");
    if (!display.flush()) {
        return fail_step(r, "flush", "flush failed");
    }

    if (!wait_idle(kLongIdleTimeoutMs)) {
        return fail_step(r, "wait_idle", "wait_idle failed");
    }

    delay(1000);
    display.clear(' ');

    if (!wait_idle(kLongIdleTimeoutMs)) {
        return fail_step(r, "wait_idle_after_clear", "clear phase failed");
    }

    add_check_bool(r, "fill->flush->clear", true, true);
    snprintf(r.note, sizeof(r.note), "fill->flush->clear");
    return true;
}

bool action_cycle_clear_chars(TestReport& r)
{
    if (!prepare_display_test()) {
        return fail_step(r, "prepare", "prepare failed");
    }

    static const char chars[] = {'1', '/', '-', '\\', '*'};

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

            if (!display.flush()) {
                char note[96];
                snprintf(note, sizeof(note), "flush failed after clear('%c')", chars[i]);
                return fail_step(r, "flush", note);
            }

            if (!wait_idle(kLongIdleTimeoutMs)) {
                char note[96];
                snprintf(note, sizeof(note), "wait_idle failed after clear('%c')", chars[i]);
                return fail_step(r, "wait_idle", note);
            }
        }
    }
}

bool action_cursor_enable(TestReport& r)
{
    if (!prepare_display_test()) {
        return fail_step(r, "prepare", "prepare failed");
    }

    if (!display.setCursorEnable(true)) {
        return fail_step(r, "setCursorEnable", "setCursorEnable(true) failed");
    }

    bool value = false;
    if (!display.getCursorEnable(&value)) {
        return fail_step(r, "getCursorEnable", "getCursorEnable() failed");
    }

    add_check_bool(r, "cursor_enable", true, value);
    snprintf(r.note, sizeof(r.note), "set/get cursor enable");
    return value;
}

bool action_cursor_visible(TestReport& r)
{
    if (!prepare_display_test()) {
        return fail_step(r, "prepare", "prepare failed");
    }

    if (!display.setCursorVisible(true)) {
        return fail_step(r, "setCursorVisible", "setCursorVisible(true) failed");
    }

    bool value = false;
    if (!display.getCursorVisible(&value)) {
        return fail_step(r, "getCursorVisible", "getCursorVisible() failed");
    }

    add_check_bool(r, "cursor_visible", true, value);
    snprintf(r.note, sizeof(r.note), "set/get cursor visible");
    return value;
}

bool action_cursor_pos(TestReport& r)
{
    if (!prepare_display_test()) {
        return fail_step(r, "prepare", "prepare failed");
    }

    const uint8_t expected = 42;
    if (!display.setCursorPos(expected)) {
        char note[96];
        snprintf(note, sizeof(note), "setCursorPos(%u) failed", expected);
        return fail_step(r, "setCursorPos", note);
    }

    uint8_t actual = 0xFF;
    if (!display.getCursorPos(&actual)) {
        return fail_step(r, "getCursorPos", "getCursorPos() failed");
    }

    add_check_eq_u8(r, "cursor_pos", expected, actual);
    snprintf(r.note, sizeof(r.note), "set/get cursor pos %u", expected);
    return actual == expected;
}

bool action_cursor_char(TestReport& r)
{
    if (!prepare_display_test()) {
        return fail_step(r, "prepare", "prepare failed");
    }

    const uint8_t expected = '_';
    if (!display.setCursorChar(static_cast<char>(expected))) {
        return fail_step(r, "setCursorChar", "setCursorChar('_') failed");
    }

    char actual = 0;
    if (!display.getCursorChar(&actual)) {
        return fail_step(r, "getCursorChar", "getCursorChar() failed");
    }

    add_check_eq_u8(r, "cursor_char", expected, static_cast<uint8_t>(actual));
    snprintf(r.note, sizeof(r.note), "set/get cursor char '_'");
    return static_cast<uint8_t>(actual) == expected;
}

bool action_cursor_blink_10ms(TestReport& r)
{
    if (!prepare_display_test()) {
        return fail_step(r, "prepare", "prepare failed");
    }

    const uint8_t expected = 25;
    if (!display.setCursorBlink10ms(expected)) {
        char note[96];
        snprintf(note, sizeof(note), "setCursorBlink10ms(%u) failed", expected);
        return fail_step(r, "setCursorBlink10ms", note);
    }

    uint8_t actual = 0xFF;
    if (!display.getCursorBlink10ms(&actual)) {
        return fail_step(r, "getCursorBlink10ms", "getCursorBlink10ms() failed");
    }

    add_check_eq_u8(r, "cursor_blink_10ms", expected, actual);
    snprintf(r.note, sizeof(r.note), "set/get cursor blink %u x10ms", expected);
    return actual == expected;
}

bool action_refresh_mode(TestReport& r)
{
    if (!prepare_display_test()) {
        return fail_step(r, "prepare", "prepare failed");
    }

    const uint8_t expected = W32DL1414_REFRESH_MODE_MANUAL;
    if (!display.setRefreshMode(expected)) {
        char note[96];
        snprintf(note, sizeof(note), "setRefreshMode(%u) failed", expected);
        return fail_step(r, "setRefreshMode", note);
    }

    uint8_t actual = 0xFF;
    if (!display.getRefreshMode(&actual)) {
        return fail_step(r, "getRefreshMode", "getRefreshMode() failed");
    }

    add_check_eq_u8(r, "refresh_mode", expected, actual);
    snprintf(r.note, sizeof(r.note), "set/get refresh mode %s", refresh_mode_text(actual));
    return actual == expected;
}

bool action_insert_mode(TestReport& r)
{
    if (!prepare_display_test()) {
        return fail_step(r, "prepare", "prepare failed");
    }

    const uint8_t expected = W32DL1414_INSERT_MODE_SCROLL_LEFT;
    if (!display.setInsertMode(expected)) {
        char note[96];
        snprintf(note, sizeof(note), "setInsertMode(%u) failed", expected);
        return fail_step(r, "setInsertMode", note);
    }

    uint8_t actual = 0xFF;
    if (!display.getInsertMode(&actual)) {
        return fail_step(r, "getInsertMode", "getInsertMode() failed");
    }

    add_check_eq_u8(r, "insert_mode", expected, actual);
    snprintf(r.note, sizeof(r.note), "set/get insert mode %s", insert_mode_text(actual));
    return actual == expected;
}

bool action_display_config(TestReport& r)
{
    if (!prepare_display_test()) {
        return fail_step(r, "prepare", "prepare failed");
    }

    W32DL1414DisplayConfig set_cfg = {};
    set_cfg.refresh_mode = W32DL1414_REFRESH_MODE_MANUAL;
    set_cfg.insert_mode = W32DL1414_INSERT_MODE_SCROLL_LEFT;

    if (!display.setDisplayConfig(set_cfg,
                                  W32DL1414_DISPLAY_CONFIG_MASK_REFRESH_MODE |
                                  W32DL1414_DISPLAY_CONFIG_MASK_INSERT_MODE)) {
        return fail_step(r, "setDisplayConfig", "setDisplayConfig() failed");
    }

    W32DL1414DisplayConfig get_cfg = {};
    if (!display.getDisplayConfig(&get_cfg)) {
        print_last_response_debug_for_opcode(W32DL1414_OPCODE_GET_DISPLAY_CONFIG);
        return fail_step(r, "getDisplayConfig", "getDisplayConfig() failed");
    }

    add_check_eq_u8(r, "disp.refresh", set_cfg.refresh_mode, get_cfg.refresh_mode);
    add_check_eq_u8(r, "disp.insert", set_cfg.insert_mode, get_cfg.insert_mode);

    snprintf(r.note, sizeof(r.note), "display cfg refresh=%s insert=%s",
             refresh_mode_text(get_cfg.refresh_mode),
             insert_mode_text(get_cfg.insert_mode));
    return true;
}

bool action_cursor_config(TestReport& r)
{
    if (!prepare_display_test()) {
        return fail_step(r, "prepare", "prepare failed");
    }

    W32DL1414CursorConfig set_cfg = {};
    set_cfg.enabled = true;
    set_cfg.visible = true;
    set_cfg.pos = 17;
    set_cfg.cursor_char = '_';
    set_cfg.blink_10ms = 25;
    set_cfg.end_of_display = 1;

    if (!display.setCursorConfig(set_cfg,
                                 W32DL1414_CURSOR_CONFIG_MASK_ENABLED |
                                 W32DL1414_CURSOR_CONFIG_MASK_VISIBLE |
                                 W32DL1414_CURSOR_CONFIG_MASK_POS |
                                 W32DL1414_CURSOR_CONFIG_MASK_CHAR |
                                 W32DL1414_CURSOR_CONFIG_MASK_BLINK_10MS |
                                 W32DL1414_CURSOR_CONFIG_MASK_END_OF_DISPLAY)) {
        return fail_step(r, "setCursorConfig", "setCursorConfig() failed");
    }

    W32DL1414CursorConfig get_cfg = {};
    if (!display.getCursorConfig(&get_cfg)) {
        print_last_response_debug_for_opcode(W32DL1414_OPCODE_GET_CURSOR_CONFIG);
        return fail_step(r, "getCursorConfig", "getCursorConfig() failed");
    }

    add_check_bool(r, "cur.enabled", set_cfg.enabled, get_cfg.enabled);
    add_check_bool(r, "cur.visible", set_cfg.visible, get_cfg.visible);
    add_check_eq_u8(r, "cur.pos", set_cfg.pos, get_cfg.pos);
    add_check_eq_u8(r, "cur.char", set_cfg.cursor_char, get_cfg.cursor_char);
    add_check_eq_u8(r, "cur.blink", set_cfg.blink_10ms, get_cfg.blink_10ms);
    add_check_eq_u8(r, "cur.eod", set_cfg.end_of_display, get_cfg.end_of_display);

    snprintf(r.note, sizeof(r.note), "cursor cfg pos=%u char=0x%02X blink=%u",
             get_cfg.pos, get_cfg.cursor_char, get_cfg.blink_10ms);
    return true;
}

bool action_read_dip(TestReport& r)
{
    uint8_t dip = 0xFF;
    const bool ok = display.readDip(&dip);

    Serial.printf("readDip=%s dip=0x%02X\n", ok ? "true" : "false", dip);
    print_last_response_debug_for_opcode(W32DL1414_OPCODE_READ_DIP);

    add_check_bool(r, "readDip", true, ok);
    if (!ok) {
        capture_fail_snapshot(r);
        snprintf(r.note, sizeof(r.note), "readDip() failed");
        return false;
    }

    add_check_range_i(r, "dip.range", 0, 7, dip);
    snprintf(r.note, sizeof(r.note), "dip=0x%02X", dip);
    return true;
}

bool action_soft_reset(TestReport& r)
{
    if (!display.softReset()) {
        return fail_step(r, "softReset", "softReset() failed");
    }

    delay(50);
    const uint8_t ping = display.ping();
    add_check_eq_u8(r, "ping_after_reset", 0xAA, ping);
    snprintf(r.note, sizeof(r.note), "soft reset");
    return ping == 0xAA;
}

bool action_hard_reset(TestReport& r)
{
    if (!display.hardReset()) {
        return fail_step(r, "hardReset", "hardReset() failed");
    }

    delay(200);

    const bool begin_ok = display.begin();
    add_check_bool(r, "begin_after_reset", true, begin_ok);
    if (!begin_ok) {
        capture_fail_snapshot(r);
        snprintf(r.note, sizeof(r.note), "begin after hard reset failed");
        return false;
    }

    const uint8_t ping = display.ping();
    add_check_eq_u8(r, "ping_after_reset", 0xAA, ping);
    snprintf(r.note, sizeof(r.note), "hard reset + begin");
    return ping == 0xAA;
}

bool action_status(TestReport& r)
{
    const W32DL1414RawStatus st = display.getStatusRaw();
    char buf[160];
    format_state(buf, sizeof(buf), st);

    add_check_bool(r, "status_read", true, true);
    snprintf(r.note, sizeof(r.note), "%s", buf);
    return true;
}

bool action_read_buf_dump(TestReport& r)
{
    if (!prepare_display_test()) {
        return fail_step(r, "prepare", "prepare failed");
    }

    constexpr uint8_t row_len = 32;
    constexpr uint8_t chunk_len = 16;
    uint8_t buf[row_len];

    Serial.println();
    Serial.println("=== VRAM BUF DUMP ===");

    for (uint8_t row = 0; row < 4; ++row) {
        const uint8_t start = static_cast<uint8_t>(row * row_len);

        for (uint8_t chunk = 0; chunk < 2; ++chunk) {
            const uint8_t chunk_start = static_cast<uint8_t>(start + chunk * chunk_len);
            if (!display.readBuf(chunk_start, &buf[chunk * chunk_len], chunk_len)) {
                char note[96];
                snprintf(note, sizeof(note), "readBuf row=%u chunk=%u failed", row, chunk);
                return fail_step(r, "readBuf", note);
            }
        }

        char ascii[row_len + 1];
        char dirty[row_len + 1];

        for (uint8_t i = 0; i < row_len; ++i) {
            const uint8_t raw = buf[i];
            const uint8_t ch = static_cast<uint8_t>(raw & 0x7F);
            ascii[i] = (ch >= 32 && ch <= 126) ? static_cast<char>(ch) : '.';
            dirty[i] = (raw & 0x80) ? 'D' : '.';
        }

        ascii[row_len] = '\0';
        dirty[row_len] = '\0';

        Serial.printf("ROW %u (%3u-%3u)\n", row, start, static_cast<uint8_t>(start + row_len - 1));
        Serial.printf("  TXT : %s\n", ascii);
        Serial.printf("  DIR : %s\n", dirty);
    }

    add_check_bool(r, "readBufDump", true, true);
    snprintf(r.note, sizeof(r.note), "readBuf dump ok");
    return true;
}

TestSpec tests[] = {
    {'1', "begin",               true,  action_begin},
    {'2', "ping",                true,  action_ping},
    {'3', "version",             true,  action_version},
    {'4', "sysinfo",             true,  action_sysinfo},
    {'5', "write_char",          false, action_write_char},
    {'6', "write_buffer",        false, action_write_buffer},
    {'7', "clear",               false, action_clear},
    {'8', "flush",               false, action_flush},
    {'9', "cursor",              true,  action_cursor},
    {'p', "print",               false, action_print},
    {'f', "printf",              false, action_printf},
    {'l', "long_printf",         false, action_long_printf},
    {'k', "fill_clear",          false, action_fill_clear},
    {'s', "status",              true,  action_status},
    {'t', "cycle_clear_chars",   false, action_cycle_clear_chars},

    {'u', "cursor_enable",       true,  action_cursor_enable},
    {'v', "cursor_visible",      true,  action_cursor_visible},
    {'m', "cursor_pos",          true,  action_cursor_pos},
    {'n', "cursor_char",         true,  action_cursor_char},
    {'b', "cursor_blink_10ms",   true,  action_cursor_blink_10ms},

    {'r', "refresh_mode",        true,  action_refresh_mode},
    {'o', "insert_mode",         true,  action_insert_mode},
    {'g', "display_config",      true,  action_display_config},
    {'c', "cursor_config",       true,  action_cursor_config},

    {'d', "read_dip",            true,  action_read_dip},
    {'y', "soft_reset",          false, action_soft_reset},
    {'x', "hard_reset",          false, action_hard_reset},
    {'z', "read_buf_dump",       false, action_read_buf_dump},
};

constexpr size_t test_count = sizeof(tests) / sizeof(tests[0]);

const TestSpec* find_test_by_key(char k)
{
    for (size_t i = 0; i < test_count; ++i) {
        if (tests[i].key == k) {
            return &tests[i];
        }
    }
    return nullptr;
}

void run_named_test_by_spec(const TestSpec& t)
{
    TestReport r;
    execute_test(t, r);

    Serial.println();
    print_compact_header();
    print_compact_row(r);
    print_separator();
    print_checks_verbose(r);
}

void run_all_tests()
{
    Serial.println();
    Serial.println("Running full test suite...");

    for (size_t i = 0; i < test_count; ++i) {
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
    Serial.println("   z  readBuf dump");
    Serial.println("   t  cycle clear chars");
    Serial.println();

    Serial.println(" Cursor:");
    Serial.println("   u  cursor enable set/get");
    Serial.println("   v  cursor visible set/get");
    Serial.println("   m  cursor pos set/get");
    Serial.println("   n  cursor char set/get");
    Serial.println("   b  cursor blink x10ms set/get");
    Serial.println("   c  full cursor config set/get");
    Serial.println();

    Serial.println(" Modes / config:");
    Serial.println("   r  refresh mode set/get");
    Serial.println("   o  insert mode set/get");
    Serial.println("   g  full display config set/get");
    Serial.println();

    Serial.println(" System:");
    Serial.println("   d  read dip");
    Serial.println("   y  soft reset");
    Serial.println("   x  hard reset");
    Serial.println("======================================================");
}

void handle_serial()
{
    while (Serial.available() > 0) {
        const char cmd = static_cast<char>(Serial.read());

        if (cmd == '\r' || cmd == '\n') {
            continue;
        }

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

        if (cmd == 's') {
            print_status_block("RAW STATUS", display.getStatusRaw());
            continue;
        }

        const TestSpec* spec = find_test_by_key(cmd);
        if (spec == nullptr) {
            Serial.println("Unknown command. Press 'h' for help.");
            continue;
        }

        run_named_test_by_spec(*spec);
    }
}
} // namespace

void run_w32dl1414_test_suite()
{
    run_all_tests();
}

void setup()
{
    Serial.begin(115200);
    while (!Serial) {
    }

    Wire.begin();
    Wire.setClock(100000);
    delay(2000);

    Serial.println();
    Serial.println("Booting W32DL1414 debug console (config-refactored)...");

    const bool ok = display.begin();
    Serial.printf("Display begin: %s\n", ok ? "PASS" : "FAIL");

    print_status_block("INITIAL STATUS", display.getStatusRaw());
    print_help();
}

void loop()
{
    handle_serial();
}
#pragma once

#include <Arduino.h>
#include <Wire.h>
#include <cstdint>

#include "../shared/W32DL1414_prtcl.h"

struct W32DL1414SystemInfo {
    uint8_t board = 0;
    uint16_t clock_khz = 0;
    uint16_t ram_total = 0;
    uint16_t ram_free = 0;
};

struct W32DL1414RawStatus {
    uint8_t engine_status = 0;
    uint8_t operation_status = 0;
    uint8_t error_code = 0;
    uint8_t job_status = 0;
    uint8_t job_type = 0;
    uint8_t job_offset = 0;
    uint8_t job_progress = 0;
};

struct W32DL1414CursorConfig {
    bool enabled = false;
    bool visible = false;
    uint8_t pos = 0;
    uint8_t cursor_char = 0;
    uint8_t blink_10ms = 0;
    uint8_t end_of_display = 0;
};

struct W32DL1414DisplayConfig {
    uint8_t refresh_mode = 0;
    uint8_t insert_mode = 0;
};

enum W32DL1414ClientError : uint8_t {
    W32DL1414_CLIENT_OK = 0,
    W32DL1414_CLIENT_WIRE_TX = 1,
    W32DL1414_CLIENT_WIRE_RX_SHORT = 2,
    W32DL1414_CLIENT_INVALID_RESULT = 3,
    W32DL1414_CLIENT_DEVICE_ERROR = 4,
    W32DL1414_CLIENT_INVALID_RESPONSE_LEN = 5,
};

const char* w32dl1414_client_error_text(uint8_t error);

class W32DL1414 {
public:
    explicit W32DL1414(uint8_t i2c_address);

    bool begin();
    uint8_t ping();

    void clear(char fill_char = W32DL1414_FILL_CHAR);
    void setCursor(uint8_t position);
    uint8_t getCursor() const;
    void putChar(char ascii);
    void print(const char* str);
    void print(uint8_t position, const char* str);
    void printf(const char* format, ...);
    void printf(uint8_t position, const char* format, ...);

    bool flush();
    bool writeChar(uint8_t position, char ascii);
    bool writeBuffer(uint8_t position, const uint8_t* data, uint8_t len);
    bool readBuf(uint8_t position, uint8_t* data, uint8_t len);
    bool waitUntilIdle(uint16_t timeout_ms = 8000);
    bool isBusy() const;
    bool isCompatible() const;

    W32DL1414SystemInfo getSystemInfo();
    W32DL1414RawStatus getStatusRaw();

    bool setCursorEnable(bool value);
    bool getCursorEnable(bool* value);
    bool setCursorVisible(bool value);
    bool getCursorVisible(bool* value);
    bool setCursorPos(uint8_t pos);
    bool getCursorPos(uint8_t* pos);
    bool setCursorChar(char c);
    bool getCursorChar(char* c);
    bool setCursorBlink10ms(uint8_t value);
    bool getCursorBlink10ms(uint8_t* value);
    bool setRefreshMode(uint8_t mode);
    bool getRefreshMode(uint8_t* value);
    bool setInsertMode(uint8_t mode);
    bool getInsertMode(uint8_t* value);

    bool setDisplayConfig(const W32DL1414DisplayConfig& cfg, uint8_t mask);
    bool getDisplayConfig(W32DL1414DisplayConfig* cfg);
    bool setCursorConfig(const W32DL1414CursorConfig& cfg, uint8_t mask);
    bool getCursorConfig(W32DL1414CursorConfig* cfg);

    bool softReset();
    bool hardReset();
    bool readDip(uint8_t* value);

    uint8_t getFirmwareMajorVersion() const;
    uint8_t getFirmwareMinorVersion() const;
    uint8_t getFirmwarePatchVersion() const;

    uint8_t getLastClientError() const;
    uint8_t getLastDeviceError() const;
    uint8_t getLastResponseLen() const;
    const uint8_t* getLastResponseBuf() const;
    uint8_t getLastWireError() const;

private:
    bool executeOpcode(uint8_t opcode,
                       const uint8_t* input_buf,
                       uint8_t input_len,
                       uint8_t* output_buf,
                       uint8_t* output_len,
                       uint8_t request_len = 8,
                       uint16_t timeout_ms = 20);
    bool pollJobStatus(uint16_t timeout_ms);
    bool parseSimpleResponse(uint8_t expected_success_len);
    bool requestSimpleOpcode(uint8_t opcode, uint8_t expected_success_len = 1);
    bool requestSimpleOpcode(uint8_t opcode,
                             const uint8_t* input_buf,
                             uint8_t input_len,
                             uint8_t expected_success_len = 1,
                             uint16_t timeout_ms = 20);
    void resetLastExchange();
    void storeLastResponse(const uint8_t* buf, uint8_t len);

    static constexpr uint8_t required_firmware_major = 0;
    static constexpr uint8_t required_firmware_minor = 1;
    static constexpr uint16_t default_job_timeout_ms = 15000;
    static constexpr uint8_t max_cached_response_len = 16;

    const uint8_t i2c_address;
    uint8_t firmware_major_version = 0;
    uint8_t firmware_minor_version = 0;
    uint8_t firmware_patch_version = 0;
    uint8_t cursor_position = 0;
    bool compatible = false;

    W32DL1414CursorConfig _cursor_config = {};
    W32DL1414DisplayConfig _display_config = {};

    uint8_t _last_client_error = W32DL1414_CLIENT_OK;
    uint8_t _last_device_error = 0x00;
    uint8_t _last_response_len = 0;
    uint8_t _last_wire_error = 0;
    uint8_t _last_response_buf[max_cached_response_len] = {0};
};
#pragma once

#include <Arduino.h>
#include <Wire.h>
#include "../shared/W32DL1414_prtcl.h"

struct W32DL1414SystemInfo
{
    uint8_t board;
    uint16_t clock_khz;
    uint16_t ram_total;
    uint16_t ram_free;
};

struct W32DL1414RawStatus
{
    uint8_t engine_status;
    uint8_t operation_status;
    uint8_t error_code;
    uint8_t job_status;
    uint8_t job_type;
    uint8_t job_offset;
    uint8_t job_progress;
};

enum W32DL1414ClientError : uint8_t {
    W32DL1414_CLIENT_OK = 0,
    W32DL1414_CLIENT_WIRE_TX = 1,
    W32DL1414_CLIENT_WIRE_RX_SHORT = 2,
    W32DL1414_CLIENT_INVALID_RESULT = 3,
    W32DL1414_CLIENT_DEVICE_ERROR = 4,
    W32DL1414_CLIENT_INVALID_RESPONSE_LEN = 5,
};

static inline const char* w32dl1414_client_error_text(uint8_t e) {
    switch (e) {
        case W32DL1414_CLIENT_OK: return "OK";
        case W32DL1414_CLIENT_WIRE_TX: return "WIRE_TX";
        case W32DL1414_CLIENT_WIRE_RX_SHORT: return "WIRE_RX_SHORT";
        case W32DL1414_CLIENT_INVALID_RESULT: return "BAD_RESULT";
        case W32DL1414_CLIENT_DEVICE_ERROR: return "DEVICE_ERR";
        case W32DL1414_CLIENT_INVALID_RESPONSE_LEN: return "BAD_LEN";
        default: return "?";
    }
}

class W32DL1414 {
public:
    explicit W32DL1414(uint8_t i2c_address);

    bool begin();
    uint8_t ping();

    void clear(char fill_char = W32DL1414_FILL_CHAR);

    uint8_t getFirmwareMajorVersion() const;
    uint8_t getFirmwareMinorVersion() const;
    uint8_t getFirmwarePatchVersion() const;

    void setCursor(uint8_t position);
    uint8_t getCursor() const;

    void putChar(char ascii);
    void print(const char* str);
    void print(uint8_t position, const char* str);
    void printf(const char* format, ...);
    void printf(uint8_t position, const char* format, ...);

    bool flush();

    W32DL1414SystemInfo getSystemInfo();
    W32DL1414RawStatus getStatusRaw();

    bool writeChar(uint8_t position, char ascii);
    bool writeBuffer(uint8_t position, const uint8_t* data, uint8_t len);

    bool waitUntilIdle(uint16_t timeout_ms = 8000);
    bool isBusy() const;
    bool isCompatible() const;

    uint8_t getLastClientError() const { return _lastClientError; }
    uint8_t getLastDeviceError() const { return _lastDeviceError; }
    uint8_t getLastResponseLen() const { return _lastResponseLen; }
    uint8_t getLastWireError() const { return _lastWireError; }

private:
    bool executeOpcode(uint8_t opcode,
                       const uint8_t* input_buf,
                       uint8_t input_len,
                       uint8_t* output_buf,
                       uint8_t* output_len,
                       uint16_t timeout_ms = 20);

    bool pollJobStatus(uint16_t timeout_ms);
    bool isRetryableBusy(uint8_t result_code) const;
    bool parseSimpleResponse(uint8_t expected_success_len);

    static constexpr uint8_t required_firmware_major = 0;
    static constexpr uint8_t required_firmware_minor = 1;

    const uint8_t i2c_address;
    uint8_t firmware_major_version = 0;
    uint8_t firmware_minor_version = 0;
    uint8_t firmware_patch_version = 0;
    uint8_t cursor_position = 0;
    bool compatible = false;

    uint8_t _lastClientError = W32DL1414_CLIENT_OK;
    uint8_t _lastDeviceError = 0x00;
    uint8_t _lastResponseLen = 0;
    uint8_t _lastWireError = 0;

    void resetLastErrors() {
        _lastClientError = W32DL1414_CLIENT_OK;
        _lastDeviceError = 0x00;
        _lastResponseLen = 0;
        _lastWireError = 0;
    }
};
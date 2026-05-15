#include "displayclass/W32DL1414.h"
#include "shared/W32DL1414_prtcl.h"
#include <Wire.h>
#include <cstdarg>
#include <cstdio>

W32DL1414::W32DL1414(uint8_t i2c_address)
: i2c_address(i2c_address)
{
}

bool W32DL1414::begin()
{
    cursor_position = 0;
    firmware_major_version = 0;
    firmware_minor_version = 0;
    firmware_patch_version = 0;
    compatible = false;

    Wire.begin();

    uint8_t output_buf[8] = {0};
    uint8_t output_len = 0;

    if (!executeOpcode(W32DL1414_OPCODE_GET_VERSION, nullptr, 0, output_buf, &output_len)) {
        return false;
    }

    if (output_len < 4) {
        return false;
    }

    if (output_buf[0] != W32DL1414_RESULT_OK) {
        return false;
    }

    firmware_major_version = output_buf[1];
    firmware_minor_version = output_buf[2];
    firmware_patch_version = output_buf[3];

    compatible = (firmware_major_version > required_firmware_major)
              || (firmware_major_version == required_firmware_major && firmware_minor_version >= required_firmware_minor);

    return true;
}

uint8_t W32DL1414::ping()
{
    uint8_t output_buf[4] = {0};
    uint8_t output_len = 0;

    if (executeOpcode(W32DL1414_OPCODE_PING, nullptr, 0, output_buf, &output_len)
        && output_len >= 2
        && output_buf[0] == W32DL1414_RESULT_OK) {
        return output_buf[1];
    }

    return 0xFF;
}

void W32DL1414::clear(char fill_char)
{
    if (!compatible) {
        return;
    }

    uint8_t input_buf[1] = { static_cast<uint8_t>(fill_char) };
    uint8_t output_buf[8] = {0};
    uint8_t output_len = 0;

    if (executeOpcode(W32DL1414_OPCODE_CLEAR, input_buf, 1, output_buf, &output_len)
        && output_len >= 1
        && output_buf[0] == W32DL1414_RESULT_OK) {
        cursor_position = 0;
    }
}

uint8_t W32DL1414::getFirmwareMajorVersion() const
{
    return firmware_major_version;
}

uint8_t W32DL1414::getFirmwareMinorVersion() const
{
    return firmware_minor_version;
}

uint8_t W32DL1414::getFirmwarePatchVersion() const
{
    return firmware_patch_version;
}

void W32DL1414::setCursor(uint8_t position)
{
    if (position < W32DL1414_DISPLAY_SIZE) {
        cursor_position = position;
    }
}

uint8_t W32DL1414::getCursor() const
{
    return cursor_position;
}

void W32DL1414::putChar(char ascii)
{
    if (!compatible) {
        return;
    }

    if (cursor_position >= W32DL1414_DISPLAY_SIZE) {
        return;
    }

    uint8_t input_buf[2] = { cursor_position, static_cast<uint8_t>(ascii) };
    uint8_t output_buf[8] = {0};
    uint8_t output_len = 0;

    if (executeOpcode(W32DL1414_OPCODE_WRITE_CHAR, input_buf, 2, output_buf, &output_len)
        && output_len >= 1
        && output_buf[0] == W32DL1414_RESULT_OK) {
        if (cursor_position + 1 < W32DL1414_DISPLAY_SIZE) {
            ++cursor_position;
        }
    }
}


void W32DL1414::print(const char* str)
{
    if (!compatible || str == nullptr) {
        return;
    }

    static constexpr uint8_t chunk_size = 8;

    while (*str != '\0' && cursor_position < W32DL1414_DISPLAY_SIZE) {
        const uint8_t start = cursor_position;
        uint8_t len = 0;

        while (str[len] != '\0' &&
               len < chunk_size &&
               (uint16_t)start + len < W32DL1414_DISPLAY_SIZE) {
            ++len;
        }

        if (len == 0) {
            return;
        }

        if (!writeBuffer(start, reinterpret_cast<const uint8_t*>(str), len)) {
            return;
        }

        cursor_position = (uint8_t)(start + len);
        str += len;
    }
}
/*
void W32DL1414::print(uint8_t position, const char* str)
{
    if (!compatible || str == nullptr || position >= W32DL1414_DISPLAY_SIZE) {
        return;
    }

    cursor_position = position;
    print(str);
}*/void W32DL1414::print(uint8_t position, const char* str)
{
    if (!compatible || str == nullptr) return;

    size_t total_len = strlen(str);
    size_t written = 0;
    size_t current_pos = position;

    const size_t safe_payload = 28;  // testweise konservativ

    Serial.printf("[print] pos=%u total_len=%u\n", position, (unsigned)total_len);

    while (written < total_len && current_pos < W32DL1414_DISPLAY_SIZE) {
        size_t remaining = total_len - written;
        size_t chunk_len = remaining < safe_payload ? remaining : safe_payload;

        if (current_pos + chunk_len > W32DL1414_DISPLAY_SIZE) {
            chunk_len = W32DL1414_DISPLAY_SIZE - current_pos;
        }

        Serial.printf("[print] cur=%u rem=%u chunk=%u\n",
            (unsigned)current_pos,
            (unsigned)remaining,
            (unsigned)chunk_len);

        if (chunk_len == 0) break;

        if (!writeBuffer((uint8_t)current_pos, (const uint8_t*)(str + written), (uint8_t)chunk_len)) {
            Serial.println("[print] writeBuffer failed");
            return;
        }

        written += chunk_len;
        current_pos += chunk_len;
    }
}

void W32DL1414::printf(const char* format, ...)
{
    if (!compatible || format == nullptr) {
        return;
    }

    char buffer[W32DL1414_DISPLAY_SIZE + 1];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    print(buffer);
}
/*
void W32DL1414::printf(uint8_t position, const char* format, ...)
{
    if (!compatible || format == nullptr || position >= W32DL1414_DISPLAY_SIZE) {
        return;
    }

    char buffer[W32DL1414_DISPLAY_SIZE + 1];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    print(position, buffer);
}*/void W32DL1414::printf(uint8_t position, const char* format, ...)
{
    if (!compatible || format == nullptr) return;

    char buffer[W32DL1414_DISPLAY_SIZE + 1];
    va_list args;
    va_start(args, format);
    int needed = vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    Serial.printf("[printf] needed=%d bufsize=%u\n", needed, (unsigned)sizeof(buffer));
    if (needed < 0) return;

    print(position, buffer);
}


bool W32DL1414::flush()
{
  if (!compatible) {
    return false;
  }

  W32DL1414RawStatus before = getStatusRaw();
  if (before.job_status == W32DL1414_JOB_STATUS_PENDING ||
      before.job_status == W32DL1414_JOB_STATUS_RUNNING) {
    if (!waitUntilIdle(15000)) {
      return false;
    }
  }

  uint8_t output_buf[8] = {0};
  uint8_t output_len = 0;

  if (!executeOpcode(W32DL1414_OPCODE_FLUSH, nullptr, 0, output_buf, &output_len)) {
    return false;
  }

  if (output_len < 1 || output_buf[0] != W32DL1414_RESULT_OK) {
    return false;
  }

  return waitUntilIdle(15000);
}


W32DL1414SystemInfo W32DL1414::getSystemInfo()
{
    W32DL1414SystemInfo info = {0};
    if (!compatible) {
        return info;
    }

    uint8_t output_buf[8] = {0};
    uint8_t output_len = 0;

    if (executeOpcode(W32DL1414_OPCODE_GET_SYSTEM_INFO, nullptr, 0, output_buf, &output_len)
        && output_len == 8
        && output_buf[0] == W32DL1414_RESULT_OK) {
        info.board = output_buf[1];
        info.clock_khz = (uint16_t)output_buf[2] | ((uint16_t)output_buf[3] << 8);
        info.ram_total = (uint16_t)output_buf[4] | ((uint16_t)output_buf[5] << 8);
        info.ram_free = (uint16_t)output_buf[6] | ((uint16_t)output_buf[7] << 8);
    }

    return info;
}

W32DL1414RawStatus W32DL1414::getStatusRaw()
{
    W32DL1414RawStatus status = {0};
    if (!compatible) {
        return status;
    }

    uint8_t output_buf[8] = {0};
    uint8_t output_len = 0;

    if (executeOpcode(W32DL1414_OPCODE_GET_STATUS, nullptr, 0, output_buf, &output_len)
        && output_len == 8
        && output_buf[0] == W32DL1414_RESULT_OK) {
        status.engine_status = output_buf[1];
        status.operation_status = output_buf[2];
        status.error_code = output_buf[3];
        status.job_status = output_buf[4];
        status.job_type = output_buf[5];
        status.job_offset = output_buf[6];
        status.job_progress = output_buf[7];
    }

    return status;
}

bool W32DL1414::writeChar(uint8_t position, char ascii)
{
    if (!compatible || position >= W32DL1414_DISPLAY_SIZE) {
        return false;
    }

    resetLastErrors();

    Wire.beginTransmission(i2c_address);
    Wire.write(W32DL1414_OPCODE_WRITE_CHAR);
    Wire.write(position);
    Wire.write((uint8_t)ascii);
    _lastWireError = Wire.endTransmission(true);

    if (_lastWireError != 0) {
        _lastClientError = W32DL1414_CLIENT_WIRE_TX;
        return false;
    }

    return parseSimpleResponse(1);
}

bool W32DL1414::writeBuffer(uint8_t position, const uint8_t* data, uint8_t len)
{
    if (!compatible || data == nullptr || len == 0) {
        return false;
    }

    if (position >= W32DL1414_DISPLAY_SIZE) {
        return false;
    }

    if ((uint16_t)position + (uint16_t)len > W32DL1414_DISPLAY_SIZE) {
        return false;
    }

    if ((uint8_t)(2 + len) > W32DL1414_MAX_TRANSFER_LEN) {
        return false;
    }

    uint8_t input_buf[W32DL1414_MAX_TRANSFER_LEN] = {0};
    input_buf[0] = position;
    input_buf[1] = len;
    for (uint8_t i = 0; i < len; ++i) {
        input_buf[2 + i] = data[i];
    }

    uint8_t output_buf[8] = {0};
    uint8_t output_len = 0;

    if (!executeOpcode(W32DL1414_OPCODE_WRITE_BUF, input_buf, (uint8_t)(2 + len), output_buf, &output_len)) {
        return false;
    }

    if (output_len < 1 || output_buf[0] != W32DL1414_RESULT_OK) {
        return false;
    }

    return waitUntilIdle(15000);
}

/*
// funktionierender stand
bool W32DL1414::writeBuffer(uint8_t position, const uint8_t* data, uint8_t len)
{
    if (!compatible || data == nullptr || len == 0) {
        return false;
    }

    if (position >= W32DL1414_DISPLAY_SIZE) {
        return false;
    }

    if ((uint16_t)position + (uint16_t)len > W32DL1414_DISPLAY_SIZE) {
        return false;
    }

    if ((uint8_t)(2 + len) > W32DL1414_MAX_TRANSFER_LEN) {
        return false;
    }

    uint8_t input_buf[W32DL1414_MAX_TRANSFER_LEN] = {0};
    input_buf[0] = position;
    input_buf[1] = len;
    for (uint8_t i = 0; i < len; ++i) {
        input_buf[2 + i] = data[i];
    }

    uint8_t output_buf[8] = {0};
    uint8_t output_len = 0;

    if (!executeOpcode(W32DL1414_OPCODE_WRITE_BUF, input_buf, (uint8_t)(2 + len), output_buf, &output_len)) {
        return false;
    }

    return output_len >= 1 && output_buf[0] == W32DL1414_RESULT_OK;
}*/

bool W32DL1414::waitUntilIdle(uint16_t timeout_ms)
{
    return pollJobStatus(timeout_ms);
}

bool W32DL1414::isBusy() const
{
    if (!compatible) {
        return false;
    }

    W32DL1414RawStatus status = const_cast<W32DL1414*>(this)->getStatusRaw();

    return status.job_status == W32DL1414_JOB_STATUS_PENDING || status.job_status == W32DL1414_JOB_STATUS_RUNNING;
}


bool W32DL1414::isCompatible() const
{
    return compatible;
}

bool W32DL1414::isRetryableBusy(uint8_t result_code) const
{
    return result_code == W32DL1414_RESULT_FAILED;
}

bool W32DL1414::pollJobStatus(uint16_t timeout_ms)
{
    const uint32_t start = millis();

    while ((uint32_t)(millis() - start) < timeout_ms) {
        W32DL1414RawStatus status = getStatusRaw();

        if (status.job_status == W32DL1414_JOB_STATUS_DONE ||
            status.job_status == W32DL1414_JOB_STATUS_NONE) {
            return true;
        }

        if (status.job_status == W32DL1414_JOB_STATUS_FAILED ||
            status.engine_status == W32DL1414_ENGINE_STATUS_ERROR) {
            return false;
        }

        delay(10);
    }

    return false;
}

bool W32DL1414::executeOpcode(uint8_t opcode,
                              const uint8_t* input_buf,
                              uint8_t input_len,
                              uint8_t* output_buf,
                              uint8_t* output_len,
                              uint16_t timeout_ms)
{
    if (output_len == nullptr) {
        return false;
    }

    *output_len = 0;

    const uint32_t start = millis();
    while (true) {
        Wire.beginTransmission(i2c_address);
        Wire.write(opcode);
        for (uint8_t i = 0; i < input_len; ++i) {
            Wire.write(input_buf[i]);
        }

        const uint8_t tx_status = Wire.endTransmission(true);
        if (tx_status == 0) {
            break;
        }

        if ((uint32_t)(millis() - start) >= timeout_ms) {
            return false;
        }

        delay(2);
    }

    delay(1);

    constexpr uint8_t request_len = 8;
    Wire.requestFrom((int)i2c_address, (int)request_len, (int)true);

    if (output_buf == nullptr) {
        while (Wire.available()) {
            (void)Wire.read();
        }
        return true;
    }

    uint8_t index = 0;
    while (Wire.available() && index < request_len) {
        output_buf[index++] = (uint8_t)Wire.read();
    }

    *output_len = index;
    return index > 0;
}

bool W32DL1414::parseSimpleResponse(uint8_t expected_success_len)
{
    resetLastErrors();

    const uint8_t requested_len = (expected_success_len < 2) ? 2 : expected_success_len;
    const uint8_t n = Wire.requestFrom((int)i2c_address, (int)requested_len, (int)true);
    _lastResponseLen = n;

    if (n == 0) {
        _lastClientError = W32DL1414_CLIENT_WIRE_RX_SHORT;
        return false;
    }

    const int first = Wire.read();
    if (first < 0) {
        _lastClientError = W32DL1414_CLIENT_WIRE_RX_SHORT;
        return false;
    }

    const uint8_t result = (uint8_t)first;

    if (result == W32DL1414_RESULT_OK) {
        while (Wire.available()) {
            (void)Wire.read();
        }
        _lastClientError = W32DL1414_CLIENT_OK;
        return true;
    }

    if (result == W32DL1414_RESULT_FAILED) {
        if (!Wire.available()) {
            _lastClientError = W32DL1414_CLIENT_INVALID_RESPONSE_LEN;
            return false;
        }

        const int second = Wire.read();
        if (second < 0) {
            _lastClientError = W32DL1414_CLIENT_INVALID_RESPONSE_LEN;
            return false;
        }

        _lastDeviceError = (uint8_t)second;

        while (Wire.available()) {
            (void)Wire.read();
        }

        _lastClientError = W32DL1414_CLIENT_DEVICE_ERROR;
        return false;
    }

    while (Wire.available()) {
        (void)Wire.read();
    }

    _lastClientError = W32DL1414_CLIENT_INVALID_RESULT;
    return false;
}
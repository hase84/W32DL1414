#include "displayclass/W32DL1414.h"
#include "shared/W32DL1414_prtcl.h"

#include <Wire.h>
#include <cstdarg>
#include <cstdio>

const char* w32dl1414_client_error_text(uint8_t error)
{
    switch (error) {
        case W32DL1414_CLIENT_OK:
            return "OK";
        case W32DL1414_CLIENT_WIRE_TX:
            return "WIRE_TX";
        case W32DL1414_CLIENT_WIRE_RX_SHORT:
            return "WIRE_RX_SHORT";
        case W32DL1414_CLIENT_INVALID_RESULT:
            return "BAD_RESULT";
        case W32DL1414_CLIENT_DEVICE_ERROR:
            return "DEVICE_ERR";
        case W32DL1414_CLIENT_INVALID_RESPONSE_LEN:
            return "BAD_LEN";
        default:
            return "?";
    }
}

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
    _cursor_config = {};
    _display_config = {};

    resetLastExchange();
    Wire.begin();

    uint8_t output_buf[8] = {0};
    uint8_t output_len = 0;

    if (!executeOpcode(W32DL1414_OPCODE_GET_VERSION, nullptr, 0, output_buf, &output_len)) {
        return false;
    }

    if (output_len < 4 || output_buf[0] != W32DL1414_RESULT_OK) {
        return false;
    }

    firmware_major_version = output_buf[1];
    firmware_minor_version = output_buf[2];
    firmware_patch_version = output_buf[3];

    compatible = (firmware_major_version > required_firmware_major)
              || (firmware_major_version == required_firmware_major
                  && firmware_minor_version >= required_firmware_minor);

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

    const uint8_t input_buf[1] = { static_cast<uint8_t>(fill_char) };
    if (requestSimpleOpcode(W32DL1414_OPCODE_CLEAR, input_buf, 1)) {
        cursor_position = 0;
        _cursor_config.pos = 0;
        _cursor_config.end_of_display = 0;
    }
}

void W32DL1414::setCursor(uint8_t position)
{
    if (position < W32DL1414_DISPLAY_SIZE) {
        cursor_position = position;
        _cursor_config.pos = position;
    }
}

uint8_t W32DL1414::getCursor() const
{
    return cursor_position;
}

void W32DL1414::putChar(char ascii)
{
    if (writeChar(cursor_position, ascii) && cursor_position + 1 < W32DL1414_DISPLAY_SIZE) {
        ++cursor_position;
        _cursor_config.pos = cursor_position;
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

        while (str[len] != '\0'
            && len < chunk_size
            && static_cast<uint16_t>(start) + len < W32DL1414_DISPLAY_SIZE) {
            ++len;
        }

        if (len == 0) {
            return;
        }

        if (!writeBuffer(start, reinterpret_cast<const uint8_t*>(str), len)) {
            return;
        }

        cursor_position = static_cast<uint8_t>(start + len);
        _cursor_config.pos = cursor_position;
        str += len;
    }
}

void W32DL1414::print(uint8_t position, const char* str)
{
    if (!compatible || str == nullptr || position >= W32DL1414_DISPLAY_SIZE) {
        return;
    }

    cursor_position = position;
    _cursor_config.pos = position;
    print(str);
}

void W32DL1414::printf(const char* format, ...)
{
    if (!compatible || format == nullptr) {
        return;
    }

    char buffer[W32DL1414_DISPLAY_SIZE + 1] = {0};
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    print(buffer);
}

void W32DL1414::printf(uint8_t position, const char* format, ...)
{
    if (!compatible || format == nullptr || position >= W32DL1414_DISPLAY_SIZE) {
        return;
    }

    char buffer[W32DL1414_DISPLAY_SIZE + 1] = {0};
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    print(position, buffer);
}

bool W32DL1414::flush()
{
    if (!compatible) {
        return false;
    }

    const W32DL1414RawStatus before = getStatusRaw();
    if (before.job_status == W32DL1414_JOB_STATUS_PENDING
        || before.job_status == W32DL1414_JOB_STATUS_RUNNING) {
        if (!waitUntilIdle(default_job_timeout_ms)) {
            return false;
        }
    }

    if (!requestSimpleOpcode(W32DL1414_OPCODE_FLUSH)) {
        return false;
    }

    return waitUntilIdle(default_job_timeout_ms);
}

bool W32DL1414::writeChar(uint8_t position, char ascii)
{
    if (!compatible || position >= W32DL1414_DISPLAY_SIZE) {
        return false;
    }

    const uint8_t input_buf[2] = { position, static_cast<uint8_t>(ascii) };
    return requestSimpleOpcode(W32DL1414_OPCODE_WRITE_CHAR, input_buf, 2);
}

bool W32DL1414::writeBuffer(uint8_t position, const uint8_t* data, uint8_t len)
{
    if (!compatible || data == nullptr || len == 0) {
        return false;
    }

    if (position >= W32DL1414_DISPLAY_SIZE) {
        return false;
    }

    if (static_cast<uint16_t>(position) + static_cast<uint16_t>(len) > W32DL1414_DISPLAY_SIZE) {
        return false;
    }

    if (static_cast<uint8_t>(2 + len) > W32DL1414_MAX_TRANSFER_LEN) {
        return false;
    }

    uint8_t input_buf[W32DL1414_MAX_TRANSFER_LEN] = {0};
    input_buf[0] = position;
    input_buf[1] = len;
    for (uint8_t i = 0; i < len; ++i) {
        input_buf[2 + i] = data[i];
    }

    if (!requestSimpleOpcode(W32DL1414_OPCODE_WRITE_BUF, input_buf, static_cast<uint8_t>(2 + len))) {
        return false;
    }

    return waitUntilIdle(default_job_timeout_ms);
}

bool W32DL1414::readBuf(uint8_t position, uint8_t* data, uint8_t len)
{
    if (!compatible || data == nullptr || len == 0) {
        return false;
    }

    if (len > W32DL1414_MAX_READ_BUF_LEN || position >= W32DL1414_DISPLAY_SIZE) {
        return false;
    }

    if (static_cast<uint16_t>(position) + static_cast<uint16_t>(len) > W32DL1414_DISPLAY_SIZE) {
        return false;
    }

    uint8_t input_buf[2] = { position, len };
    uint8_t output_buf[W32DL1414_MAX_TRANSFER_LEN] = {0};
    uint8_t output_len = 0;

    if (!executeOpcode(W32DL1414_OPCODE_READ_BUF,
                       input_buf,
                       2,
                       output_buf,
                       &output_len,
                       static_cast<uint8_t>(2 + len))) {
        return false;
    }

    if (output_len != static_cast<uint8_t>(2 + len) || output_buf[0] != W32DL1414_RESULT_OK) {
        return false;
    }

    if (output_buf[1] != len) {
        _last_client_error = W32DL1414_CLIENT_INVALID_RESPONSE_LEN;
        return false;
    }

    for (uint8_t i = 0; i < len; ++i) {
        data[i] = output_buf[2 + i];
    }

    return true;
}

bool W32DL1414::waitUntilIdle(uint16_t timeout_ms)
{
    return pollJobStatus(timeout_ms);
}

bool W32DL1414::isBusy() const
{
    if (!compatible) {
        return false;
    }

    const W32DL1414RawStatus status = const_cast<W32DL1414*>(this)->getStatusRaw();
    return status.job_status == W32DL1414_JOB_STATUS_PENDING
        || status.job_status == W32DL1414_JOB_STATUS_RUNNING;
}

bool W32DL1414::isCompatible() const
{
    return compatible;
}

W32DL1414SystemInfo W32DL1414::getSystemInfo()
{
    W32DL1414SystemInfo info;
    if (!compatible) {
        return info;
    }

    uint8_t output_buf[8] = {0};
    uint8_t output_len = 0;

    if (!executeOpcode(W32DL1414_OPCODE_GET_SYSTEM_INFO, nullptr, 0, output_buf, &output_len)) {
        return info;
    }

    if (output_len != 8 || output_buf[0] != W32DL1414_RESULT_OK) {
        return info;
    }

    info.board = output_buf[1];
    info.clock_khz = static_cast<uint16_t>(output_buf[2]) | (static_cast<uint16_t>(output_buf[3]) << 8);
    info.ram_total = static_cast<uint16_t>(output_buf[4]) | (static_cast<uint16_t>(output_buf[5]) << 8);
    info.ram_free = static_cast<uint16_t>(output_buf[6]) | (static_cast<uint16_t>(output_buf[7]) << 8);

    return info;
}

W32DL1414RawStatus W32DL1414::getStatusRaw()
{
    W32DL1414RawStatus status;
    if (!compatible) {
        return status;
    }

    uint8_t output_buf[8] = {0};
    uint8_t output_len = 0;

    if (!executeOpcode(W32DL1414_OPCODE_GET_STATUS, nullptr, 0, output_buf, &output_len)) {
        return status;
    }

    if (output_len != 8 || output_buf[0] != W32DL1414_RESULT_OK) {
        return status;
    }

    status.engine_status = output_buf[1];
    status.operation_status = output_buf[2];
    status.error_code = output_buf[3];
    status.job_status = output_buf[4];
    status.job_type = output_buf[5];
    status.job_offset = output_buf[6];
    status.job_progress = output_buf[7];

    return status;
}

bool W32DL1414::setDisplayConfig(const W32DL1414DisplayConfig& cfg, uint8_t mask)
{
    if (!compatible) {
        return false;
    }

    const uint8_t input_buf[3] = {
        mask,
        cfg.refresh_mode,
        cfg.insert_mode
    };

    if (!requestSimpleOpcode(W32DL1414_OPCODE_SET_DISPLAY_CONFIG, input_buf, 3, 1)) {
        return false;
    }

    if ((mask & W32DL1414_DISPLAY_CONFIG_MASK_REFRESH_MODE) != 0U) {
        _display_config.refresh_mode = cfg.refresh_mode;
    }

    if ((mask & W32DL1414_DISPLAY_CONFIG_MASK_INSERT_MODE) != 0U) {
        _display_config.insert_mode = cfg.insert_mode;
    }

    return true;
}

bool W32DL1414::getDisplayConfig(W32DL1414DisplayConfig* cfg)
{
    if (!compatible || cfg == nullptr) {
        return false;
    }

    uint8_t output_buf[8] = {0};
    uint8_t output_len = 0;

    if (!executeOpcode(W32DL1414_OPCODE_GET_DISPLAY_CONFIG, nullptr, 0, output_buf, &output_len, 8)) {
        return false;
    }

    if (output_len < 5 || output_buf[0] != W32DL1414_RESULT_OK) {
        _last_client_error = W32DL1414_CLIENT_INVALID_RESPONSE_LEN;
        return false;
    }

    const uint8_t count = output_buf[1];
    if (count < 3) {
        _last_client_error = W32DL1414_CLIENT_INVALID_RESPONSE_LEN;
        return false;
    }

    const uint8_t mask = output_buf[2];

    if ((mask & W32DL1414_DISPLAY_CONFIG_MASK_REFRESH_MODE) != 0U) {
        cfg->refresh_mode = output_buf[3];
        _display_config.refresh_mode = cfg->refresh_mode;
    }

    if ((mask & W32DL1414_DISPLAY_CONFIG_MASK_INSERT_MODE) != 0U) {
        cfg->insert_mode = output_buf[4];
        _display_config.insert_mode = cfg->insert_mode;
    }

    return true;
}

bool W32DL1414::setCursorConfig(const W32DL1414CursorConfig& cfg, uint8_t mask)
{
    if (!compatible) {
        return false;
    }

    const uint8_t input_buf[7] = {
        mask,
        static_cast<uint8_t>(cfg.enabled ? 1U : 0U),
        static_cast<uint8_t>(cfg.visible ? 1U : 0U),
        cfg.pos,
        cfg.cursor_char,
        cfg.blink_10ms,
        cfg.end_of_display
    };

    if (!requestSimpleOpcode(W32DL1414_OPCODE_SET_CURSOR_CONFIG, input_buf, 7, 1)) {
        return false;
    }

    if ((mask & W32DL1414_CURSOR_CONFIG_MASK_ENABLED) != 0U) {
        _cursor_config.enabled = cfg.enabled;
    }

    if ((mask & W32DL1414_CURSOR_CONFIG_MASK_VISIBLE) != 0U) {
        _cursor_config.visible = cfg.visible;
    }

    if ((mask & W32DL1414_CURSOR_CONFIG_MASK_POS) != 0U) {
        _cursor_config.pos = cfg.pos;
        cursor_position = cfg.pos;
    }

    if ((mask & W32DL1414_CURSOR_CONFIG_MASK_CHAR) != 0U) {
        _cursor_config.cursor_char = cfg.cursor_char;
    }

    if ((mask & W32DL1414_CURSOR_CONFIG_MASK_BLINK_10MS) != 0U) {
        _cursor_config.blink_10ms = cfg.blink_10ms;
    }

    if ((mask & W32DL1414_CURSOR_CONFIG_MASK_END_OF_DISPLAY) != 0U) {
        _cursor_config.end_of_display = cfg.end_of_display;
    }

    return true;
}

bool W32DL1414::getCursorConfig(W32DL1414CursorConfig* cfg)
{
    if (!compatible || cfg == nullptr) {
        return false;
    }

    uint8_t output_buf[10] = {0};
    uint8_t output_len = 0;

    if (!executeOpcode(W32DL1414_OPCODE_GET_CURSOR_CONFIG, nullptr, 0, output_buf, &output_len, 10)) {
        return false;
    }

    if (output_len < 9 || output_buf[0] != W32DL1414_RESULT_OK) {
        _last_client_error = W32DL1414_CLIENT_INVALID_RESPONSE_LEN;
        return false;
    }

    const uint8_t count = output_buf[1];
    if (count < 7) {
        _last_client_error = W32DL1414_CLIENT_INVALID_RESPONSE_LEN;
        return false;
    }

    const uint8_t mask = output_buf[2];

    if ((mask & W32DL1414_CURSOR_CONFIG_MASK_ENABLED) != 0U) {
        cfg->enabled = (output_buf[3] != 0U);
        _cursor_config.enabled = cfg->enabled;
    }

    if ((mask & W32DL1414_CURSOR_CONFIG_MASK_VISIBLE) != 0U) {
        cfg->visible = (output_buf[4] != 0U);
        _cursor_config.visible = cfg->visible;
    }

    if ((mask & W32DL1414_CURSOR_CONFIG_MASK_POS) != 0U) {
        cfg->pos = output_buf[5];
        _cursor_config.pos = cfg->pos;
        cursor_position = cfg->pos;
    }

    if ((mask & W32DL1414_CURSOR_CONFIG_MASK_CHAR) != 0U) {
        cfg->cursor_char = output_buf[6];
        _cursor_config.cursor_char = cfg->cursor_char;
    }

    if ((mask & W32DL1414_CURSOR_CONFIG_MASK_BLINK_10MS) != 0U) {
        cfg->blink_10ms = output_buf[7];
        _cursor_config.blink_10ms = cfg->blink_10ms;
    }

    if ((mask & W32DL1414_CURSOR_CONFIG_MASK_END_OF_DISPLAY) != 0U) {
        cfg->end_of_display = output_buf[8];
        _cursor_config.end_of_display = cfg->end_of_display;
    }

    return true;
}

bool W32DL1414::setCursorEnable(bool value)
{
    W32DL1414CursorConfig cfg = _cursor_config;
    cfg.enabled = value;
    if (!value) {
        cfg.end_of_display = 0;
    }
    return setCursorConfig(cfg, W32DL1414_CURSOR_CONFIG_MASK_ENABLED | W32DL1414_CURSOR_CONFIG_MASK_END_OF_DISPLAY);
}

bool W32DL1414::getCursorEnable(bool* value)
{
    if (value == nullptr) {
        return false;
    }

    W32DL1414CursorConfig cfg;
    if (!getCursorConfig(&cfg)) {
        return false;
    }

    *value = cfg.enabled;
    return true;
}

bool W32DL1414::setCursorVisible(bool value)
{
    W32DL1414CursorConfig cfg = _cursor_config;
    cfg.visible = value;
    return setCursorConfig(cfg, W32DL1414_CURSOR_CONFIG_MASK_VISIBLE);
}

bool W32DL1414::getCursorVisible(bool* value)
{
    if (value == nullptr) {
        return false;
    }

    W32DL1414CursorConfig cfg;
    if (!getCursorConfig(&cfg)) {
        return false;
    }

    *value = cfg.visible;
    return true;
}

bool W32DL1414::setCursorPos(uint8_t pos)
{
    if (pos >= W32DL1414_DISPLAY_SIZE) {
        return false;
    }

    W32DL1414CursorConfig cfg = _cursor_config;
    cfg.pos = pos;
    return setCursorConfig(cfg, W32DL1414_CURSOR_CONFIG_MASK_POS);
}

bool W32DL1414::getCursorPos(uint8_t* pos)
{
    if (pos == nullptr) {
        return false;
    }

    W32DL1414CursorConfig cfg;
    if (!getCursorConfig(&cfg)) {
        return false;
    }

    *pos = cfg.pos;
    return true;
}

bool W32DL1414::setCursorChar(char c)
{
    W32DL1414CursorConfig cfg = _cursor_config;
    cfg.cursor_char = static_cast<uint8_t>(c);
    return setCursorConfig(cfg, W32DL1414_CURSOR_CONFIG_MASK_CHAR);
}

bool W32DL1414::getCursorChar(char* c)
{
    if (c == nullptr) {
        return false;
    }

    W32DL1414CursorConfig cfg;
    if (!getCursorConfig(&cfg)) {
        return false;
    }

    *c = static_cast<char>(cfg.cursor_char);
    return true;
}

bool W32DL1414::setCursorBlink10ms(uint8_t value)
{
    W32DL1414CursorConfig cfg = _cursor_config;
    cfg.blink_10ms = value;
    return setCursorConfig(cfg, W32DL1414_CURSOR_CONFIG_MASK_BLINK_10MS);
}

bool W32DL1414::getCursorBlink10ms(uint8_t* value)
{
    if (value == nullptr) {
        return false;
    }

    W32DL1414CursorConfig cfg;
    if (!getCursorConfig(&cfg)) {
        return false;
    }

    *value = cfg.blink_10ms;
    return true;
}

bool W32DL1414::setRefreshMode(uint8_t mode)
{
    W32DL1414DisplayConfig cfg = _display_config;
    cfg.refresh_mode = mode;
    return setDisplayConfig(cfg, W32DL1414_DISPLAY_CONFIG_MASK_REFRESH_MODE);
}

bool W32DL1414::getRefreshMode(uint8_t* value)
{
    if (value == nullptr) {
        return false;
    }

    W32DL1414DisplayConfig cfg;
    if (!getDisplayConfig(&cfg)) {
        return false;
    }

    *value = cfg.refresh_mode;
    return true;
}

bool W32DL1414::setInsertMode(uint8_t mode)
{
    W32DL1414DisplayConfig cfg = _display_config;
    cfg.insert_mode = mode;
    return setDisplayConfig(cfg, W32DL1414_DISPLAY_CONFIG_MASK_INSERT_MODE);
}

bool W32DL1414::getInsertMode(uint8_t* value)
{
    if (value == nullptr) {
        return false;
    }

    W32DL1414DisplayConfig cfg;
    if (!getDisplayConfig(&cfg)) {
        return false;
    }

    *value = cfg.insert_mode;
    return true;
}


bool W32DL1414::softReset()
{
    return requestSimpleOpcode(W32DL1414_OPCODE_SOFT_RESET);
}

bool W32DL1414::hardReset()
{
    if (!compatible) {
        return false;
    }

    _last_response_len = 0;
    for (uint8_t i = 0; i < sizeof(_last_response_buf); ++i) {
        _last_response_buf[i] = 0;
    }

    Wire.beginTransmission(i2c_address);
    Wire.write(W32DL1414_OPCODE_HARD_RESET);

    const uint8_t tx_status = Wire.endTransmission(true);
    if (tx_status != 0) {
        return false;
    }

    delay(200);
    return true;
}

bool W32DL1414::readDip(uint8_t* value)
{
    if (!compatible || value == nullptr) {
        return false;
    }

    uint8_t output_buf[8] = {0};
    uint8_t output_len = 0;

    if (!executeOpcode(W32DL1414_OPCODE_READ_DIP, nullptr, 0, output_buf, &output_len)) {
        return false;
    }

    if (output_len < 2 || output_buf[0] != W32DL1414_RESULT_OK) {
        if (output_len < 2 && _last_client_error == W32DL1414_CLIENT_OK) {
            _last_client_error = W32DL1414_CLIENT_INVALID_RESPONSE_LEN;
        }
        return false;
    }

    *value = output_buf[1];
    return true;
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

uint8_t W32DL1414::getLastClientError() const
{
    return _last_client_error;
}

uint8_t W32DL1414::getLastDeviceError() const
{
    return _last_device_error;
}

uint8_t W32DL1414::getLastResponseLen() const
{
    return _last_response_len;
}

const uint8_t* W32DL1414::getLastResponseBuf() const
{
    return _last_response_buf;
}

uint8_t W32DL1414::getLastWireError() const
{
    return _last_wire_error;
}

bool W32DL1414::pollJobStatus(uint16_t timeout_ms)
{
    const uint32_t start = millis();
    while (static_cast<uint32_t>(millis() - start) < timeout_ms) {
        const W32DL1414RawStatus status = getStatusRaw();

        if (status.job_status == W32DL1414_JOB_STATUS_DONE
            || status.job_status == W32DL1414_JOB_STATUS_NONE) {
            return true;
        }

        if (status.job_status == W32DL1414_JOB_STATUS_FAILED
            || status.engine_status == W32DL1414_ENGINE_STATUS_ERROR) {
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
                              uint8_t request_len,
                              uint16_t timeout_ms)
{
    if (output_len == nullptr) {
        return false;
    }

    resetLastExchange();
    *output_len = 0;

    const uint32_t start = millis();
    while (true) {
        Wire.beginTransmission(i2c_address);
        Wire.write(opcode);

        for (uint8_t i = 0; i < input_len; ++i) {
            Wire.write(input_buf[i]);
        }

        _last_wire_error = Wire.endTransmission(true);
        if (_last_wire_error == 0) {
            break;
        }

        if (static_cast<uint32_t>(millis() - start) >= timeout_ms) {
            _last_client_error = W32DL1414_CLIENT_WIRE_TX;
            return false;
        }

        delay(2);
    }

    delay(1);
    Wire.requestFrom(static_cast<int>(i2c_address), static_cast<int>(request_len), static_cast<int>(true));

    if (output_buf == nullptr) {
        uint8_t temp_buf[max_cached_response_len] = {0};
        uint8_t index = 0;

        while (Wire.available()) {
            const uint8_t value = static_cast<uint8_t>(Wire.read());
            if (index < max_cached_response_len) {
                temp_buf[index] = value;
            }
            ++index;
        }

        storeLastResponse(temp_buf, index);

        if (index == 0) {
            _last_client_error = W32DL1414_CLIENT_WIRE_RX_SHORT;
            return false;
        }

        return true;
    }

    uint8_t index = 0;
    while (Wire.available() && index < request_len) {
        output_buf[index++] = static_cast<uint8_t>(Wire.read());
    }

    *output_len = index;
    storeLastResponse(output_buf, index);

    if (index == 0) {
        _last_client_error = W32DL1414_CLIENT_WIRE_RX_SHORT;
        return false;
    }

    const uint8_t result = output_buf[0];
    if (result == W32DL1414_RESULT_OK) {
        _last_client_error = W32DL1414_CLIENT_OK;
        return true;
    }

    if (result == W32DL1414_RESULT_FAILED) {
        if (index >= 2) {
            _last_device_error = output_buf[1];
            _last_client_error = W32DL1414_CLIENT_DEVICE_ERROR;
        } else {
            _last_client_error = W32DL1414_CLIENT_INVALID_RESPONSE_LEN;
        }
        return false;
    }

    _last_client_error = W32DL1414_CLIENT_INVALID_RESULT;
    return false;
}

bool W32DL1414::parseSimpleResponse(uint8_t expected_success_len)
{
    if (_last_response_len == 0) {
        _last_client_error = W32DL1414_CLIENT_WIRE_RX_SHORT;
        return false;
    }

    const uint8_t result = _last_response_buf[0];
    if (result == W32DL1414_RESULT_FAILED) {
        if (_last_response_len >= 2) {
            _last_device_error = _last_response_buf[1];
            _last_client_error = W32DL1414_CLIENT_DEVICE_ERROR;
        } else {
            _last_client_error = W32DL1414_CLIENT_INVALID_RESPONSE_LEN;
        }
        return false;
    }

    if (result != W32DL1414_RESULT_OK) {
        _last_client_error = W32DL1414_CLIENT_INVALID_RESULT;
        return false;
    }

    if (_last_response_len < expected_success_len) {
        _last_client_error = W32DL1414_CLIENT_INVALID_RESPONSE_LEN;
        return false;
    }

    _last_client_error = W32DL1414_CLIENT_OK;
    return true;
}

bool W32DL1414::requestSimpleOpcode(uint8_t opcode, uint8_t expected_success_len)
{
    return requestSimpleOpcode(opcode, nullptr, 0, expected_success_len);
}

bool W32DL1414::requestSimpleOpcode(uint8_t opcode,
                                    const uint8_t* input_buf,
                                    uint8_t input_len,
                                    uint8_t expected_success_len,
                                    uint16_t timeout_ms)
{
    uint8_t output_buf[8] = {0};
    uint8_t output_len = 0;

    if (!executeOpcode(opcode, input_buf, input_len, output_buf, &output_len, 8, timeout_ms)) {
        return false;
    }

    return parseSimpleResponse(expected_success_len);
}

void W32DL1414::resetLastExchange()
{
    _last_client_error = W32DL1414_CLIENT_OK;
    _last_device_error = 0x00;
    _last_response_len = 0;
    _last_wire_error = 0;

    for (uint8_t i = 0; i < max_cached_response_len; ++i) {
        _last_response_buf[i] = 0;
    }
}

void W32DL1414::storeLastResponse(const uint8_t* buf, uint8_t len)
{
    _last_response_len = (len > max_cached_response_len) ? max_cached_response_len : len;

    for (uint8_t i = 0; i < max_cached_response_len; ++i) {
        _last_response_buf[i] = 0;
    }

    if (buf == nullptr) {
        return;
    }

    for (uint8_t i = 0; i < _last_response_len; ++i) {
        _last_response_buf[i] = buf[i];
    }
}
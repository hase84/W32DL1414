#include "firmware/version.h"
#include "firmware/opcodes.h"
#include "firmware/board.h"
#include "firmware/engine.h"
#include "firmware/vram.h"
#include "shared/W32DL1414_prtcl.h"
#include <stdint.h>

#ifndef FW_VER_MAJOR
#define FW_VER_MAJOR 0
#endif

#ifndef FW_VER_MINOR
#define FW_VER_MINOR 0
#endif

#ifndef FW_VER_PATCH
#define FW_VER_PATCH 0
#endif

static inline void begin_operation(uint8_t opcode)
{
    engine_start_operation(opcode);
}

static inline void finish_operation(bool successful, uint8_t error_code = W32DL1414_ERROR_NONE)
{
    engine_finish_operation(successful, error_code);
}

void opcode_handle_ping(volatile uint8_t input_len,
                        const uint8_t* input_buf,
                        uint8_t* output_len,
                        uint8_t* output_buf)
{
    (void)input_len;
    (void)input_buf;

    begin_operation(W32DL1414_OPCODE_PING);
    engine_response_begin(output_len, output_buf, true, W32DL1414_ERROR_NONE);
    engine_response_push_u8(output_len, output_buf, 0xAA);
    finish_operation(true);
}

void opcode_handle_get_version(volatile uint8_t input_len,
                               const uint8_t* input_buf,
                               uint8_t* output_len,
                               uint8_t* output_buf)
{
    (void)input_len;
    (void)input_buf;

    begin_operation(W32DL1414_OPCODE_GET_VERSION);
    engine_response_begin(output_len, output_buf, true, W32DL1414_ERROR_NONE);
    engine_response_push_u8(output_len, output_buf, FW_VER_MAJOR);
    engine_response_push_u8(output_len, output_buf, FW_VER_MINOR);
    engine_response_push_u8(output_len, output_buf, FW_VER_PATCH);
    finish_operation(true);
}

void opcode_handle_get_status(volatile uint8_t input_len,
                              const uint8_t* input_buf,
                              uint8_t* output_len,
                              uint8_t* output_buf)
{
    (void)input_len;
    (void)input_buf;

    engine_state_t* state = engine_get_state();
    if (state == nullptr) {
        engine_response_begin(output_len, output_buf, false, W32DL1414_ERROR_INTERNAL);
        return;
    }

    engine_response_begin(output_len, output_buf, true, W32DL1414_ERROR_NONE);
    engine_response_push_u8(output_len, output_buf, state->status);
    engine_response_push_u8(output_len, output_buf, state->operation.status);
    engine_response_push_u8(output_len, output_buf, state->operation.error_code);
    engine_response_push_u8(output_len, output_buf, state->job.status);
    engine_response_push_u8(output_len, output_buf, state->job.type);
    engine_response_push_u8(output_len, output_buf, state->job.offset);
    engine_response_push_u8(output_len, output_buf, state->job.progress);
}


void opcode_handle_get_system_info(volatile uint8_t input_len,
                                   const uint8_t* input_buf,
                                   uint8_t* output_len,
                                   uint8_t* output_buf)
{
    (void)input_len;
    (void)input_buf;

    begin_operation(W32DL1414_OPCODE_GET_SYSTEM_INFO);
    engine_response_begin(output_len, output_buf, true, W32DL1414_ERROR_NONE);

    // 6 Bytes statt 8 (kein Overflow!)
    engine_response_push_u8(output_len, output_buf, 
        W32DL1414_BOARD_ATTINY44 == 1 ? W32DL1414_BOARD_ID_ATTINY44 :
        (W32DL1414_BOARD_ATTINY84 == 1 ? W32DL1414_BOARD_ID_ATTINY84 : W32DL1414_BOARD_ID_UNKNOWN));
    engine_response_push_u16le(output_len, output_buf, (uint16_t)(W32DL1414_CLOCK_SPEED_HZ / 1000UL));
    engine_response_push_u16le(output_len, output_buf, W32DL1414_RAM_TOTAL_BYTES);
    engine_response_push_u16le(output_len, output_buf, sys_calc_free_ram_bytes());

    finish_operation(true);
}


void opcode_handle_flush(volatile uint8_t input_len,
                         const uint8_t* input_buf,
                         uint8_t* output_len,
                         uint8_t* output_buf)
{
    (void)input_len;
    (void)input_buf;

    begin_operation(W32DL1414_OPCODE_FLUSH);

    engine_start_job(W32DL1414_JOB_TYPE_FLUSH_VRAM, W32DL1414_DISPLAY_SIZE);
    engine_set_job_args(0, W32DL1414_DISPLAY_SIZE);

    engine_response_begin(output_len, output_buf, true, W32DL1414_ERROR_NONE);
    finish_operation(true);
}

void opcode_handle_clear(volatile uint8_t input_len,
                         const uint8_t* input_buf,
                         uint8_t* output_len,
                         uint8_t* output_buf)
{
    uint8_t fill = W32DL1414_FILL_CHAR;

    begin_operation(W32DL1414_OPCODE_CLEAR);

    // Genaue Längenprüfung VOR dem Zugriff
    if (input_len != 2) {
        engine_response_begin(output_len, output_buf, false, W32DL1414_ERROR_INVALID_ARGUMENT);
        finish_operation(false, W32DL1414_ERROR_INVALID_ARGUMENT);
        return;
    }

    if (input_buf == nullptr) {
        engine_response_begin(output_len, output_buf, false, W32DL1414_ERROR_INVALID_ARGUMENT);
        finish_operation(false, W32DL1414_ERROR_INVALID_ARGUMENT);
        return;
    }

    fill = input_buf[1];  // input_buf[0] = Opcode, input_buf[1] = fill_char

    engine_start_job(W32DL1414_JOB_TYPE_CLEAR_VRAM, W32DL1414_DISPLAY_SIZE);
    engine_set_job_args(fill, W32DL1414_DISPLAY_SIZE);

    engine_response_begin(output_len, output_buf, true, W32DL1414_ERROR_NONE);
    finish_operation(true);
}

void opcode_handle_write_char(volatile uint8_t input_len,
                              const uint8_t* input_buf,
                              uint8_t* output_len,
                              uint8_t* output_buf)
{
    begin_operation(W32DL1414_OPCODE_WRITE_CHAR);

    if (input_buf == nullptr || input_len != 3) {
        engine_response_begin(output_len, output_buf, false, W32DL1414_ERROR_INVALID_ARGUMENT);
        finish_operation(false, W32DL1414_ERROR_INVALID_ARGUMENT);
        return;
    }

    const uint8_t position = input_buf[1];
    const uint8_t ascii = input_buf[2];

    if (position >= W32DL1414_DISPLAY_SIZE) {
        engine_response_begin(output_len, output_buf, false, W32DL1414_ERROR_OUT_OF_RANGE);
        finish_operation(false, W32DL1414_ERROR_OUT_OF_RANGE);
        return;
    }

    vram_write(position, ascii);

    engine_response_begin(output_len, output_buf, true, W32DL1414_ERROR_NONE);
    finish_operation(true);
}

void opcode_handle_write_buf(volatile uint8_t input_len,
                             const uint8_t* input_buf,
                             uint8_t* output_len,
                             uint8_t* output_buf)
{
    begin_operation(W32DL1414_OPCODE_WRITE_BUF);

    if (input_buf == nullptr || input_len < 4) {
        engine_response_begin(output_len, output_buf, false, W32DL1414_ERROR_INVALID_ARGUMENT);
        finish_operation(false, W32DL1414_ERROR_INVALID_ARGUMENT);
        return;
    }

    const uint8_t start = input_buf[1];
    const uint8_t len = input_buf[2];

    if (len == 0) {
        engine_response_begin(output_len, output_buf, true, W32DL1414_ERROR_NONE);
        finish_operation(true);
        return;
    }

    if ((uint16_t)start + (uint16_t)len > W32DL1414_DISPLAY_SIZE) {
        engine_response_begin(output_len, output_buf, false, W32DL1414_ERROR_OUT_OF_RANGE);
        finish_operation(false, W32DL1414_ERROR_OUT_OF_RANGE);
        return;
    }

    if ((uint8_t)(3 + len) > input_len) {
        engine_response_begin(output_len, output_buf, false, W32DL1414_ERROR_INVALID_ARGUMENT);
        finish_operation(false, W32DL1414_ERROR_INVALID_ARGUMENT);
        return;
    }

    for (uint8_t i = 0; i < len; ++i) {
        vram_write((uint8_t)(start + i), input_buf[3 + i]);
    }

    engine_response_begin(output_len, output_buf, true, W32DL1414_ERROR_NONE);
    finish_operation(true);
}
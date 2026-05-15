#include "firmware/version.h"
#include "firmware/opcodes.h"
#include "firmware/board.h"
#include "firmware/engine.h"
#include "firmware/vram.h"
#include "firmware/input_reader.h"
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

void opcode_handle_ping(volatile uint8_t input_len, const uint8_t* input_buf, uint8_t* output_len, uint8_t* output_buf)
{
    (void)input_len;
    (void)input_buf;

    engine_start_operation(W32DL1414_OPCODE_PING);
    engine_response_begin(output_len, output_buf, true, W32DL1414_ERROR_NONE);
    engine_response_push_u8(output_len, output_buf, 0xAA);
    engine_finish_operation(true, W32DL1414_ERROR_NONE);
}

void opcode_handle_get_version(volatile uint8_t input_len, const uint8_t* input_buf, uint8_t* output_len, uint8_t* output_buf)
{
    (void)input_len;
    (void)input_buf;

    engine_start_operation(W32DL1414_OPCODE_GET_VERSION);
    engine_response_begin(output_len, output_buf, true, W32DL1414_ERROR_NONE);
    engine_response_push_u8(output_len, output_buf, FW_VER_MAJOR);
    engine_response_push_u8(output_len, output_buf, FW_VER_MINOR);
    engine_response_push_u8(output_len, output_buf, FW_VER_PATCH);
    engine_finish_operation(true, W32DL1414_ERROR_NONE);
}

void opcode_handle_get_status(volatile uint8_t input_len, const uint8_t* input_buf, uint8_t* output_len, uint8_t* output_buf)
{
    (void)input_len;
    (void)input_buf;

    engine_start_operation(W32DL1414_OPCODE_GET_STATUS);

    const engine_state_t* state = engine_get_preserved_state();
    if (state == nullptr) {
        engine_response_begin(output_len, output_buf, false, W32DL1414_ERROR_INTERNAL);
        engine_finish_operation(false, W32DL1414_ERROR_INTERNAL);
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

    engine_finish_operation(true, W32DL1414_ERROR_NONE);
}


void opcode_handle_get_system_info(volatile uint8_t input_len, const uint8_t* input_buf, uint8_t* output_len, uint8_t* output_buf)
{
    (void)input_len;
    (void)input_buf;

    engine_start_operation(W32DL1414_OPCODE_GET_SYSTEM_INFO);
    engine_response_begin(output_len, output_buf, true, W32DL1414_ERROR_NONE);

    engine_response_push_u8(output_len, output_buf, 
        W32DL1414_BOARD_ATTINY44 == 1 ? W32DL1414_BOARD_ID_ATTINY44 :
        (W32DL1414_BOARD_ATTINY84 == 1 ? W32DL1414_BOARD_ID_ATTINY84 : W32DL1414_BOARD_ID_UNKNOWN));
    engine_response_push_u16le(output_len, output_buf, (uint16_t)(W32DL1414_CLOCK_SPEED_HZ / 1000UL));
    engine_response_push_u16le(output_len, output_buf, W32DL1414_RAM_TOTAL_BYTES);
    engine_response_push_u16le(output_len, output_buf, sys_calc_free_ram_bytes());

    engine_finish_operation(true, W32DL1414_ERROR_NONE);
}


void opcode_handle_flush(volatile uint8_t input_len, const uint8_t* input_buf, uint8_t* output_len, uint8_t* output_buf)
{
    (void)input_len;
    (void)input_buf;

    engine_start_operation(W32DL1414_OPCODE_FLUSH);

    engine_start_job(W32DL1414_JOB_TYPE_FLUSH_VRAM, W32DL1414_DISPLAY_SIZE);
    engine_set_job_args(0, W32DL1414_DISPLAY_SIZE);

    engine_response_begin(output_len, output_buf, true, W32DL1414_ERROR_NONE);
    engine_finish_operation(true, W32DL1414_ERROR_NONE);
}


void opcode_handle_clear(volatile uint8_t input_len, const uint8_t* input_buf, uint8_t* output_len, uint8_t* output_buf)
{

    engine_start_operation(W32DL1414_OPCODE_CLEAR);

    input_reader_t in;
    input_reader_init(&in, input_buf, input_len);
    input_skip_opcode(&in);

    uint8_t fill_char = get_input_u8(&in);
    if (fill_char == 0) {
        fill_char = W32DL1414_FILL_CHAR;
    }

    engine_start_job(W32DL1414_JOB_TYPE_CLEAR_VRAM, W32DL1414_DISPLAY_SIZE);
    engine_set_job_args(fill_char, W32DL1414_DISPLAY_SIZE);

    engine_response_begin(output_len, output_buf, true, W32DL1414_ERROR_NONE);
    engine_finish_operation(true, W32DL1414_ERROR_NONE);
}

void opcode_handle_write_char(volatile uint8_t input_len, const uint8_t* input_buf, uint8_t* output_len, uint8_t* output_buf)
{
    engine_start_operation(W32DL1414_OPCODE_WRITE_CHAR);

    input_reader_t in;
    input_reader_init(&in, input_buf, input_len);
    input_skip_opcode(&in);

    const uint8_t position = get_input_u8(&in);
    const uint8_t ascii = get_input_u8(&in);

    if (position >= W32DL1414_DISPLAY_SIZE) {
        engine_response_begin(output_len, output_buf, false, W32DL1414_ERROR_OUT_OF_RANGE);
        engine_finish_operation(false, W32DL1414_ERROR_OUT_OF_RANGE);
        return;
    }

    vram_write(position, ascii);

    engine_response_begin(output_len, output_buf, true, W32DL1414_ERROR_NONE);
    engine_finish_operation(true, W32DL1414_ERROR_NONE);
}

void opcode_handle_write_buf(volatile uint8_t input_len, const uint8_t* input_buf, uint8_t* output_len, uint8_t* output_buf)
{
    engine_start_operation(W32DL1414_OPCODE_WRITE_BUF);

    input_reader_t in;
    input_reader_init(&in, input_buf, input_len);
    input_skip_opcode(&in);

    const uint8_t start = get_input_u8(&in);
    const uint8_t len = get_input_u8(&in);

    // len=0 is invalid, as opcode requires at least 1 byte buffer as argument #4 and we want to avoid no-op writes that might interfere with job processing logic
    if (len == 0) {
        engine_response_begin(output_len, output_buf, false, W32DL1414_ERROR_INVALID_ARGUMENT);
        engine_finish_operation(false, W32DL1414_ERROR_INVALID_ARGUMENT);
        return;
    }

    // for the time being, do not allow writes that exceed display size; later either trucate or scroll in
    if ((uint16_t)start + (uint16_t)len > W32DL1414_DISPLAY_SIZE) {
        engine_response_begin(output_len, output_buf, false, W32DL1414_ERROR_OUT_OF_RANGE);
        engine_finish_operation(false, W32DL1414_ERROR_OUT_OF_RANGE);
        return;
    }

    for (uint8_t i = 0; i < len; ++i) {
        vram_write((uint8_t)(start + i), get_input_u8(&in));
    }

    engine_response_begin(output_len, output_buf, true, W32DL1414_ERROR_NONE);
    engine_finish_operation(true, W32DL1414_ERROR_NONE);
}



void opcode_handle_set_cursor_enable(volatile uint8_t input_len, const uint8_t* input_buf, uint8_t* output_len, uint8_t* output_buf) 
{
    (void)input_len;
    (void)input_buf;
    engine_start_operation(W32DL1414_OPCODE_SET_CURSOR_ENABLE);
    engine_response_begin(output_len, output_buf, false, W32DL1414_ERROR_NOT_IMPLEMENTED);
    engine_finish_operation(false, W32DL1414_ERROR_NOT_IMPLEMENTED);
}


void opcode_handle_get_cursor_enable(volatile uint8_t input_len, const uint8_t* input_buf, uint8_t* output_len, uint8_t* output_buf) 
{
    (void)input_len;
    (void)input_buf;
    engine_start_operation(W32DL1414_OPCODE_GET_CURSOR_ENABLE);
    engine_response_begin(output_len, output_buf, false, W32DL1414_ERROR_NOT_IMPLEMENTED);
    engine_finish_operation(false, W32DL1414_ERROR_NOT_IMPLEMENTED);

}


void opcode_handle_set_cursor_visible(volatile uint8_t input_len, const uint8_t* input_buf, uint8_t* output_len, uint8_t* output_buf) 
{
    (void)input_len;
    (void)input_buf;
    engine_start_operation(W32DL1414_OPCODE_SET_CURSOR_VISIBLE);
    engine_response_begin(output_len, output_buf, false, W32DL1414_ERROR_NOT_IMPLEMENTED);
    engine_finish_operation(false, W32DL1414_ERROR_NOT_IMPLEMENTED);
}


void opcode_handle_get_cursor_visible(volatile uint8_t input_len, const uint8_t* input_buf, uint8_t* output_len, uint8_t* output_buf) 
{
    (void)input_len;
    (void)input_buf;
    engine_start_operation(W32DL1414_OPCODE_GET_CURSOR_VISIBLE);
    engine_response_begin(output_len, output_buf, false, W32DL1414_ERROR_NOT_IMPLEMENTED);
    engine_finish_operation(false, W32DL1414_ERROR_NOT_IMPLEMENTED);
}

void opcode_handle_set_cursor_pos(volatile uint8_t input_len, const uint8_t* input_buf, uint8_t* output_len, uint8_t* output_buf) 
{
    (void)input_len;
    (void)input_buf;
    engine_start_operation(W32DL1414_OPCODE_SET_CURSOR_POS);
    engine_response_begin(output_len, output_buf, false, W32DL1414_ERROR_NOT_IMPLEMENTED);
    engine_finish_operation(false, W32DL1414_ERROR_NOT_IMPLEMENTED);
}


void opcode_handle_get_cursor_pos(volatile uint8_t input_len, const uint8_t* input_buf, uint8_t* output_len, uint8_t* output_buf) 
{
    (void)input_len;
    (void)input_buf;
    engine_start_operation(W32DL1414_OPCODE_GET_CURSOR_POS);
    engine_response_begin(output_len, output_buf, false, W32DL1414_ERROR_NOT_IMPLEMENTED);
    engine_finish_operation(false, W32DL1414_ERROR_NOT_IMPLEMENTED);
}


void opcode_handle_set_cursor_char(volatile uint8_t input_len, const uint8_t* input_buf, uint8_t* output_len, uint8_t* output_buf) 
{
    (void)input_len;
    (void)input_buf;
    engine_start_operation(W32DL1414_OPCODE_SET_CURSOR_CHAR);
    engine_response_begin(output_len, output_buf, false, W32DL1414_ERROR_NOT_IMPLEMENTED);
    engine_finish_operation(false, W32DL1414_ERROR_NOT_IMPLEMENTED);
}


void opcode_handle_get_cursor_char(volatile uint8_t input_len, const uint8_t* input_buf, uint8_t* output_len, uint8_t* output_buf) 
{
    (void)input_len;
    (void)input_buf;
    engine_start_operation(W32DL1414_OPCODE_GET_CURSOR_CHAR);
    engine_response_begin(output_len, output_buf, false, W32DL1414_ERROR_NOT_IMPLEMENTED);
    engine_finish_operation(false, W32DL1414_ERROR_NOT_IMPLEMENTED);
}


void opcode_handle_set_cursor_blink_freq(volatile uint8_t input_len, const uint8_t* input_buf, uint8_t* output_len, uint8_t* output_buf) 
{
    (void)input_len;
    (void)input_buf;
    engine_start_operation(W32DL1414_OPCODE_SET_CURSOR_BLINK_FREQ);
    engine_response_begin(output_len, output_buf, false, W32DL1414_ERROR_NOT_IMPLEMENTED);
    engine_finish_operation(false, W32DL1414_ERROR_NOT_IMPLEMENTED);
}

void opcode_handle_get_cursor_blink_freq(volatile uint8_t input_len, const uint8_t* input_buf, uint8_t* output_len, uint8_t* output_buf) 
{
    (void)input_len;
    (void)input_buf;
    engine_start_operation(W32DL1414_OPCODE_GET_CURSOR_BLINK_FREQ);
    engine_response_begin(output_len, output_buf, false, W32DL1414_ERROR_NOT_IMPLEMENTED);
    engine_finish_operation(false, W32DL1414_ERROR_NOT_IMPLEMENTED);
}

void opcode_handle_set_refresh_mode(volatile uint8_t input_len, const uint8_t* input_buf, uint8_t* output_len, uint8_t* output_buf) 
{
    (void)input_len;
    (void)input_buf;
    engine_start_operation(W32DL1414_OPCODE_SET_REFRESH_MODE);
    engine_response_begin(output_len, output_buf, false, W32DL1414_ERROR_NOT_IMPLEMENTED);
    engine_finish_operation(false, W32DL1414_ERROR_NOT_IMPLEMENTED);
}

void opcode_handle_get_refresh_mode(volatile uint8_t input_len, const uint8_t* input_buf, uint8_t* output_len, uint8_t* output_buf) 
{
    (void)input_len;
    (void)input_buf;
    engine_start_operation(W32DL1414_OPCODE_GET_REFRESH_MODE);
    engine_response_begin(output_len, output_buf, false, W32DL1414_ERROR_NOT_IMPLEMENTED);
    engine_finish_operation(false, W32DL1414_ERROR_NOT_IMPLEMENTED);
}

void opcode_handle_set_scroll_mode(volatile uint8_t input_len, const uint8_t* input_buf, uint8_t* output_len, uint8_t* output_buf) 
{
    (void)input_len;
    (void)input_buf;
    engine_start_operation(W32DL1414_OPCODE_SET_SCROLL_MODE);
    engine_response_begin(output_len, output_buf, false, W32DL1414_ERROR_NOT_IMPLEMENTED);
    engine_finish_operation(false, W32DL1414_ERROR_NOT_IMPLEMENTED);
}

void opcode_handle_get_scroll_mode(volatile uint8_t input_len, const uint8_t *input_buf, uint8_t *output_len,uint8_t *output_buf) 
{
    (void)input_len;
    (void)input_buf;
    engine_start_operation(W32DL1414_OPCODE_GET_SCROLL_MODE);
    engine_response_begin(output_len, output_buf, false, W32DL1414_ERROR_NOT_IMPLEMENTED);
    engine_finish_operation(false, W32DL1414_ERROR_NOT_IMPLEMENTED);
}
void opcode_handle_soft_reset(volatile uint8_t input_len, const uint8_t* input_buf, uint8_t* output_len, uint8_t* output_buf) 
{
    (void)input_len;
    (void)input_buf;
    engine_start_operation(W32DL1414_OPCODE_SOFT_RESET);
    engine_response_begin(output_len, output_buf, false, W32DL1414_ERROR_NOT_IMPLEMENTED);
    engine_finish_operation(false, W32DL1414_ERROR_NOT_IMPLEMENTED);
}


void opcode_handle_hard_reset(volatile uint8_t input_len, const uint8_t* input_buf, uint8_t* output_len, uint8_t* output_buf) 
{
    (void)input_len;
    (void)input_buf;
    engine_start_operation(W32DL1414_OPCODE_HARD_RESET);
    engine_response_begin(output_len, output_buf, false, W32DL1414_ERROR_NOT_IMPLEMENTED);
    engine_finish_operation(false, W32DL1414_ERROR_NOT_IMPLEMENTED);
}


void opcode_handle_read_dip(volatile uint8_t input_len, const uint8_t* input_buf, uint8_t* output_len, uint8_t* output_buf) 
{
    (void)input_len;
    (void)input_buf;
    engine_start_operation(W32DL1414_OPCODE_READ_DIP);
    engine_response_begin(output_len, output_buf, false, W32DL1414_ERROR_NOT_IMPLEMENTED);
    engine_finish_operation(false, W32DL1414_ERROR_NOT_IMPLEMENTED);
}
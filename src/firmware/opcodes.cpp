
#include "firmware/version.h"
#include "firmware/opcodes.h"
#include "firmware/board.h"
#include "firmware/engine.h"
#include "firmware/vram.h"
#include "firmware/input_reader.h"
#include "shared/W32DL1414_prtcl.h"
#include <stdint.h>
#include <avr/wdt.h>


#ifndef FW_VER_MAJOR
#define FW_VER_MAJOR 0
#endif

#ifndef FW_VER_MINOR
#define FW_VER_MINOR 0
#endif

#ifndef FW_VER_PATCH
#define FW_VER_PATCH 0
#endif


/*
 * Helper functions for opcode handlers to build responses in a consistent way.
 */
static void response_begin_fail(uint8_t* output_len, uint8_t* output_buf, uint8_t error_code)
{
    engine_response_begin(output_len, output_buf, false, error_code);
}

static void response_begin_ok_data(uint8_t* output_len, uint8_t* output_buf, uint8_t count)
{
    engine_response_begin(output_len, output_buf, true, W32DL1414_ERROR_NONE);
    engine_response_push_u8(output_len, output_buf, count);
}

static void response_begin_ok_mutation(uint8_t* output_len, uint8_t* output_buf, uint8_t requested, uint8_t processed, uint8_t changed, uint8_t flags)
{
    engine_response_begin(output_len, output_buf, true, W32DL1414_ERROR_NONE);
    engine_response_push_u8(output_len, output_buf, requested);
    engine_response_push_u8(output_len, output_buf, processed);
    engine_response_push_u8(output_len, output_buf, changed);
    engine_response_push_u8(output_len, output_buf, flags);
}


/*
 * Validation helper functions for opcode handlers.
 */
static bool is_valid_refresh_mode(uint8_t value)
{
    return value == W32DL1414_REFRESH_MODE_AUTO ||
           value == W32DL1414_REFRESH_MODE_MANUAL;
}

static bool is_valid_insert_mode(uint8_t value)
{
    return value == W32DL1414_INSERT_MODE_SCROLL_LEFT ||
           value == W32DL1414_INSERT_MODE_SCROLL_UP ||
           value == W32DL1414_INSERT_MODE_TRUNCATE ||
           value == W32DL1414_INSERT_MODE_FLIP_OVER;
}

/*
 *
 * Opcode handlers
 */

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
        response_begin_fail(output_len, output_buf, W32DL1414_ERROR_OUT_OF_RANGE);
        engine_finish_operation(false, W32DL1414_ERROR_OUT_OF_RANGE);
        return;
    }

    const bool changed = vram_write(position, ascii);

    response_begin_ok_mutation(output_len,
                               output_buf,
                               1,                                      // requested
                               1,                                      // processed
                               changed ? 1 : 0,                        // changed
                               W32DL1414_WRITE_FLAG_NONE);             // flags

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
    if (start + len > W32DL1414_DISPLAY_SIZE) {
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


void opcode_handle_read_buf(volatile uint8_t input_len, const uint8_t* input_buf, uint8_t* output_len, uint8_t* output_buf)
{
    engine_start_operation(W32DL1414_OPCODE_READ_BUF);

    input_reader_t in;
    input_reader_init(&in, input_buf, input_len);
    input_skip_opcode(&in);

    uint8_t start = get_input_u8(&in);
    uint8_t len = get_input_u8(&in);


    if (len == 0) {
        engine_response_begin(output_len, output_buf, false, W32DL1414_ERROR_INVALID_ARGUMENT);
        engine_finish_operation(false, W32DL1414_ERROR_INVALID_ARGUMENT);
        return;
    }

    if (start >= W32DL1414_DISPLAY_SIZE || start + len > W32DL1414_DISPLAY_SIZE) {
        engine_response_begin(output_len, output_buf, false, W32DL1414_ERROR_OUT_OF_RANGE);
        engine_finish_operation(false, W32DL1414_ERROR_OUT_OF_RANGE);
        return;
    }

    engine_response_begin(output_len, output_buf, true, W32DL1414_ERROR_NONE);
    
    engine_response_push_u8(output_len, output_buf, len);

    for (uint8_t i = 0; i < len; ++i) {
        engine_response_push_u8(output_len, output_buf, vram_read(start + i));
    }

    engine_finish_operation(true, W32DL1414_ERROR_NONE);
}



void opcode_handle_set_display_config(volatile uint8_t input_len, const uint8_t* input_buf, uint8_t* output_len, uint8_t* output_buf)
{
    engine_start_operation(W32DL1414_OPCODE_SET_DISPLAY_CONFIG);

    input_reader_t in;
    input_reader_init(&in, input_buf, input_len);
    input_skip_opcode(&in);

    struct W32DL1414_display_config cfg;
    vram_get_display_config(&cfg);

    const uint8_t mask = get_input_u8(&in);
    const uint8_t refresh_mode = get_input_u8(&in);
    const uint8_t insert_mode = get_input_u8(&in);

    if ((mask & W32DL1414_DISPLAY_CONFIG_MASK_REFRESH_MODE) != 0U) {
        if (!is_valid_refresh_mode(refresh_mode)) {
            response_begin_fail(output_len, output_buf, W32DL1414_ERROR_INVALID_ARGUMENT);
            engine_finish_operation(false, W32DL1414_ERROR_INVALID_ARGUMENT);
            return;
        }
        cfg.refresh_mode = refresh_mode;
    }

    if ((mask & W32DL1414_DISPLAY_CONFIG_MASK_INSERT_MODE) != 0U) {
        if (!is_valid_insert_mode(insert_mode)) {
            response_begin_fail(output_len, output_buf, W32DL1414_ERROR_INVALID_ARGUMENT);
            engine_finish_operation(false, W32DL1414_ERROR_INVALID_ARGUMENT);
            return;
        }
        cfg.insert_mode = insert_mode;
    }

    if (!vram_set_display_config(&cfg)) {
        response_begin_fail(output_len, output_buf, W32DL1414_ERROR_INTERNAL);
        engine_finish_operation(false, W32DL1414_ERROR_INTERNAL);
        return;
    }

    engine_response_begin(output_len, output_buf, true, W32DL1414_ERROR_NONE);
    engine_finish_operation(true, W32DL1414_ERROR_NONE);
}


void opcode_handle_get_display_config(volatile uint8_t input_len, const uint8_t* input_buf, uint8_t* output_len, uint8_t* output_buf)
{
    (void)input_len;
    (void)input_buf;

    engine_start_operation(W32DL1414_OPCODE_GET_DISPLAY_CONFIG);

    struct W32DL1414_display_config cfg;
    vram_get_display_config(&cfg);

    response_begin_ok_data(output_len, output_buf, 3);
    engine_response_push_u8(output_len, output_buf,
        W32DL1414_DISPLAY_CONFIG_MASK_REFRESH_MODE |
        W32DL1414_DISPLAY_CONFIG_MASK_INSERT_MODE);
    engine_response_push_u8(output_len, output_buf, cfg.refresh_mode);
    engine_response_push_u8(output_len, output_buf, cfg.insert_mode);

    engine_finish_operation(true, W32DL1414_ERROR_NONE);
}


void opcode_handle_set_cursor_config(volatile uint8_t input_len, const uint8_t* input_buf, uint8_t* output_len, uint8_t* output_buf)
{
    engine_start_operation(W32DL1414_OPCODE_SET_CURSOR_CONFIG);

    input_reader_t in;
    input_reader_init(&in, input_buf, input_len);
    input_skip_opcode(&in);

    struct W32DL1414_cursor_config cfg;
    vram_get_cursor_config(&cfg);

    const uint8_t mask = get_input_u8(&in);
    const uint8_t enabled_raw = get_input_u8(&in);
    const uint8_t visible_raw = get_input_u8(&in);
    const uint8_t pos_raw = get_input_u8(&in);
    const uint8_t char_raw = get_input_u8(&in);
    const uint8_t blink_raw = get_input_u8(&in);
    const uint8_t eod_raw = get_input_u8(&in);

    if ((mask & W32DL1414_CURSOR_CONFIG_MASK_POS) != 0U) {
        if (pos_raw >= W32DL1414_DISPLAY_SIZE) {
            response_begin_fail(output_len, output_buf, W32DL1414_ERROR_OUT_OF_RANGE);
            engine_finish_operation(false, W32DL1414_ERROR_OUT_OF_RANGE);
            return;
        }
    }

    if ((mask & W32DL1414_CURSOR_CONFIG_MASK_ENABLED) != 0U) {
        cfg.enabled = (enabled_raw != 0U);
    }

    if ((mask & W32DL1414_CURSOR_CONFIG_MASK_VISIBLE) != 0U) {
        cfg.visible = (visible_raw != 0U);
    }

    if ((mask & W32DL1414_CURSOR_CONFIG_MASK_POS) != 0U) {
        cfg.pos = pos_raw;
    }

    if ((mask & W32DL1414_CURSOR_CONFIG_MASK_CHAR) != 0U) {
        cfg.cursor_char = char_raw;
    }

    if ((mask & W32DL1414_CURSOR_CONFIG_MASK_BLINK_10MS) != 0U) {
        cfg.blink_10ms = blink_raw;
    }

    if ((mask & W32DL1414_CURSOR_CONFIG_MASK_END_OF_DISPLAY) != 0U) {
        cfg.end_of_display = (eod_raw != 0U) ? 1U : 0U;
    }

    if (!cfg.enabled) {
        cfg.end_of_display = 0;
    }

    if (!vram_set_cursor_config(&cfg)) {
        response_begin_fail(output_len, output_buf, W32DL1414_ERROR_INTERNAL);
        engine_finish_operation(false, W32DL1414_ERROR_INTERNAL);
        return;
    }

    engine_response_begin(output_len, output_buf, true, W32DL1414_ERROR_NONE);
    engine_finish_operation(true, W32DL1414_ERROR_NONE);
}


void opcode_handle_get_cursor_config(volatile uint8_t input_len, const uint8_t* input_buf, uint8_t* output_len, uint8_t* output_buf)
{
    (void)input_len;
    (void)input_buf;

    engine_start_operation(W32DL1414_OPCODE_GET_CURSOR_CONFIG);

    struct W32DL1414_cursor_config cfg;
    vram_get_cursor_config(&cfg);

    response_begin_ok_data(output_len, output_buf, 7);
    engine_response_push_u8(output_len, output_buf,
        W32DL1414_CURSOR_CONFIG_MASK_ENABLED |
        W32DL1414_CURSOR_CONFIG_MASK_VISIBLE |
        W32DL1414_CURSOR_CONFIG_MASK_POS |
        W32DL1414_CURSOR_CONFIG_MASK_CHAR |
        W32DL1414_CURSOR_CONFIG_MASK_BLINK_10MS |
        W32DL1414_CURSOR_CONFIG_MASK_END_OF_DISPLAY);
    engine_response_push_u8(output_len, output_buf, cfg.enabled ? 1U : 0U);
    engine_response_push_u8(output_len, output_buf, cfg.visible ? 1U : 0U);
    engine_response_push_u8(output_len, output_buf, cfg.pos);
    engine_response_push_u8(output_len, output_buf, cfg.cursor_char);
    engine_response_push_u8(output_len, output_buf, cfg.blink_10ms);
    engine_response_push_u8(output_len, output_buf, cfg.end_of_display ? 1U : 0U);

    engine_finish_operation(true, W32DL1414_ERROR_NONE);
}

void opcode_handle_soft_reset(volatile uint8_t input_len, const uint8_t* input_buf, uint8_t* output_len, uint8_t* output_buf) 
{
    (void)input_len;
    (void)input_buf;

    engine_start_operation(W32DL1414_OPCODE_SOFT_RESET);

    engine_reset();

    engine_response_begin(output_len, output_buf, true, W32DL1414_ERROR_NONE);
    engine_finish_operation(true, W32DL1414_ERROR_NONE);
}


void opcode_handle_hard_reset(volatile uint8_t input_len, const uint8_t* input_buf, uint8_t* output_len, uint8_t* output_buf) 
{
    (void)input_len;
    (void)input_buf;

    engine_start_operation(W32DL1414_OPCODE_HARD_RESET);

    engine_response_begin(output_len, output_buf, true, W32DL1414_ERROR_NONE);
    engine_finish_operation(true, W32DL1414_ERROR_NONE);

    schedule_hardware_reset(WDTO_15MS);
}


void opcode_handle_read_dip(volatile uint8_t input_len, const uint8_t* input_buf, uint8_t* output_len, uint8_t* output_buf) 
{
    (void)input_len;
    (void)input_buf;

    engine_start_operation(W32DL1414_OPCODE_READ_DIP);

    uint8_t dip = read_dip_switches();

    engine_response_begin(output_len, output_buf, true, W32DL1414_ERROR_NONE);
    engine_response_push_u8(output_len, output_buf, dip);
    engine_finish_operation(true, W32DL1414_ERROR_NONE);
}
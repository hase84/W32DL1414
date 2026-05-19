#pragma once

#include <stdint.h>



typedef void (*opcode_handler_fn)(volatile uint8_t input_len,
                                  const uint8_t* input_buf,
                                  uint8_t* output_len,
                                  uint8_t* output_buf);

struct opcode_handler_entry_t {
    uint8_t opcode;
    uint8_t flags;
    uint8_t min_input_len;
    uint8_t max_input_len;
    opcode_handler_fn handler;
};

#define OPCODE_FLAG_NONE               0x00
#define OPCODE_FLAG_ALLOWED_DURING_JOB 0x01


void opcode_handle_ping(volatile uint8_t input_len, const uint8_t* input_buf, uint8_t* output_len, uint8_t* output_buf);
void opcode_handle_get_version(volatile uint8_t input_len, const uint8_t* input_buf, uint8_t* output_len, uint8_t* output_buf);
void opcode_handle_get_status(volatile uint8_t input_len, const uint8_t* input_buf, uint8_t* output_len, uint8_t* output_buf);
void opcode_handle_get_system_info(volatile uint8_t input_len, const uint8_t* input_buf, uint8_t* output_len, uint8_t* output_buf);
void opcode_handle_flush(volatile uint8_t input_len, const uint8_t* input_buf, uint8_t* output_len, uint8_t* output_buf);
void opcode_handle_clear(volatile uint8_t input_len, const uint8_t* input_buf, uint8_t* output_len, uint8_t* output_buf);
void opcode_handle_write_char(volatile uint8_t input_len, const uint8_t* input_buf, uint8_t* output_len, uint8_t* output_buf);
void opcode_handle_write_buf(volatile uint8_t input_len, const uint8_t* input_buf, uint8_t* output_len, uint8_t* output_buf);
void opcode_handle_read_buf(volatile uint8_t input_len, const uint8_t* input_buf, uint8_t* output_len, uint8_t* output_buf);
void opcode_handle_set_display_config(volatile uint8_t input_len, const uint8_t* input_buf, uint8_t* output_len, uint8_t* output_buf);
void opcode_handle_get_display_config(volatile uint8_t input_len, const uint8_t* input_buf, uint8_t* output_len, uint8_t* output_buf);
void opcode_handle_set_cursor_config(volatile uint8_t input_len, const uint8_t* input_buf, uint8_t* output_len, uint8_t* output_buf);
void opcode_handle_get_cursor_config(volatile uint8_t input_len, const uint8_t* input_buf, uint8_t* output_len, uint8_t* output_buf);
void opcode_handle_soft_reset(volatile uint8_t input_len, const uint8_t* input_buf, uint8_t* output_len, uint8_t* output_buf);
void opcode_handle_hard_reset(volatile uint8_t input_len, const uint8_t* input_buf, uint8_t* output_len, uint8_t* output_buf);
void opcode_handle_read_dip(volatile uint8_t input_len, const uint8_t* input_buf, uint8_t* output_len, uint8_t* output_buf);
#pragma once

#include <stdint.h>

typedef struct {
    const uint8_t* buf;
    uint8_t len;
    uint8_t pos;
} input_reader_t;

void input_reader_init(input_reader_t* r, const uint8_t* buf, uint8_t len);
void input_skip_opcode(input_reader_t* r);
uint8_t get_input_u8(input_reader_t* r);
uint16_t get_input_u16le(input_reader_t* r);
void get_input_bytes(input_reader_t* r, uint8_t* dst, uint8_t count);
#include "firmware/input_reader.h"

void input_reader_init(input_reader_t* r, const uint8_t* buf, uint8_t len)
{
    r->buf = buf;
    r->len = len;
    r->pos = 0;
}

void input_skip_opcode(input_reader_t* r)
{
    r->pos = 1;
}

uint8_t get_input_u8(input_reader_t* r)
{
    return r->buf[r->pos++];
}

uint16_t get_input_u16le(input_reader_t* r)
{
    const uint8_t lo = r->buf[r->pos++];
    const uint8_t hi = r->buf[r->pos++];
    return (uint16_t)lo | ((uint16_t)hi << 8);
}

void get_input_bytes(input_reader_t* r, uint8_t* dst, uint8_t count)
{
    for (uint8_t i = 0; i < count; ++i) {
        dst[i] = r->buf[r->pos++];
    }
}
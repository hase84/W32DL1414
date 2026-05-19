#include "firmware/vram.h"
#include "shared/W32DL1414_prtcl.h"

#include <string.h>



uint8_t vram[W32DL1414_DISPLAY_SIZE];

static struct W32DL1414_display_config display_config;
static struct W32DL1414_cursor_config cursor_config;



void vram_init(void)
{
    memset(vram, W32DL1414_FILL_CHAR, sizeof(vram));
}

bool vram_is_dirty(uint8_t position)
{
    if (position >= W32DL1414_DISPLAY_SIZE) {
        return false;
    }

    return vram[position] & 0x80;
}

void vram_set_dirty(uint8_t position)
{
    if (position >= W32DL1414_DISPLAY_SIZE) {
        return;
    }

    vram[position] |= 0x80;
}

void vram_clear_dirty(uint8_t position)
{
    if (position >= W32DL1414_DISPLAY_SIZE) {
        return;
    }

    vram[position] &= 0x7F;
}

bool vram_write(uint8_t position, uint8_t ascii)
{
    if (position >= W32DL1414_DISPLAY_SIZE) {
        return false;
    }

    if (vram[position] != ascii) {
        vram[position] = ascii;
        vram_set_dirty(position);
        return true;
    }
    
    return false;   
}

uint8_t vram_read(uint8_t position)
{
    return vram[position];
}

uint8_t vram_read_ascii(uint8_t position)
{
    return vram[position] & 0x7F;
}


void vram_get_display_config(struct W32DL1414_display_config* out)
{
    if (out == nullptr) {
        return;
    }
    *out = display_config;
}

bool vram_set_display_config(const struct W32DL1414_display_config* in)
{
    if (in == nullptr) {
        return false;
    }

    display_config = *in;
    return true;
}

void vram_get_cursor_config(struct W32DL1414_cursor_config* out)
{
    if (out == nullptr) {
        return;
    }
    *out = cursor_config;
}

bool vram_set_cursor_config(const struct W32DL1414_cursor_config* in)
{
    if (in == nullptr) {
        return false;
    }

    cursor_config = *in;

    if (!cursor_config.enabled) {
        cursor_config.end_of_display = 0;
    }

    return true;
}

uint8_t vram_get_cursor_pos(void)
{
    return cursor_config.pos;
}

bool vram_set_cursor_pos(uint8_t pos)
{
    if (pos >= W32DL1414_DISPLAY_SIZE) {
        return false;
    }

    cursor_config.pos = pos;
    return true;
}

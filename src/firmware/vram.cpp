#include "firmware/vram.h"

#include <string.h>

uint8_t vram[W32DL1414_DISPLAY_SIZE];
uint8_t vram_dirty_flags[W32DL1414_DISPLAY_SIZE / 8];

static uint8_t dirty_mask(uint8_t position)
{
    return (uint8_t)(1U << (position & 0x07));
}

static uint8_t dirty_index(uint8_t position)
{
    return (uint8_t)(position >> 3);
}

void vram_init(void)
{
    memset(vram, W32DL1414_FILL_CHAR, sizeof(vram));
    memset(vram_dirty_flags, 0xFF, sizeof(vram_dirty_flags));
}

bool vram_is_dirty(uint8_t position)
{
    if (position >= W32DL1414_DISPLAY_SIZE) {
        return false;
    }

    return (vram_dirty_flags[dirty_index(position)] & dirty_mask(position)) != 0;
}

void vram_set_dirty(uint8_t position)
{
    if (position >= W32DL1414_DISPLAY_SIZE) {
        return;
    }

    vram_dirty_flags[dirty_index(position)] |= dirty_mask(position);
}

void vram_clear_dirty(uint8_t position)
{
    if (position >= W32DL1414_DISPLAY_SIZE) {
        return;
    }

    vram_dirty_flags[dirty_index(position)] &= (uint8_t)~dirty_mask(position);
}

void vram_write(uint8_t position, uint8_t ascii)
{
    if (position >= W32DL1414_DISPLAY_SIZE) {
        return;
    }

    if (vram[position] != ascii) {
        vram[position] = ascii;
        vram_set_dirty(position);
    }
}

uint8_t vram_read(uint8_t position)
{
    if (position >= W32DL1414_DISPLAY_SIZE) {
        return W32DL1414_FILL_CHAR;
    }

    return vram[position];
}
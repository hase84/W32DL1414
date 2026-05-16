#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "shared/W32DL1414_prtcl.h"

extern uint8_t vram[W32DL1414_DISPLAY_SIZE];
extern uint8_t vram_dirty_flags[W32DL1414_DISPLAY_SIZE / 8];

void vram_init(void);
bool vram_is_dirty(uint8_t position);
void vram_set_dirty(uint8_t position);
void vram_clear_dirty(uint8_t position);
void vram_write(uint8_t position, uint8_t ascii);
uint8_t vram_read(uint8_t position);
uint8_t vram_read_ascii(uint8_t position);
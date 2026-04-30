#pragma once

#include <stdint.h>
#include "firmware/engine.h"

typedef bool (*job_tick_fn)(engine_state_t* state);

struct job_handler_entry_t {
    uint8_t job_type;
    job_tick_fn tick;
};

bool job_tick_flush_vram(engine_state_t* state);
bool job_tick_clear_vram(engine_state_t* state);



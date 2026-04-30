#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "firmware/engine.h"
#include "firmware/opcodes.h"
#include "firmware/jobs.h"

typedef bool (*job_tick_fn)(engine_state_t* state);

typedef struct
{
    uint8_t job_type;
    job_tick_fn tick;
} job_handler_entry;

const opcode_handler_entry_t* find_opcode_handler(uint8_t opcode);
const job_handler_entry_t* find_job_handler(uint8_t job_type);
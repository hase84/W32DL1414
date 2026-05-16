#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "firmware/engine.h"
#include "firmware/opcodes.h"
#include "firmware/jobs.h"


bool find_opcode_handler(uint8_t opcode, opcode_handler_entry_t* out_entry);
bool find_job_handler(uint8_t job_type, job_handler_entry_t* out_entry);
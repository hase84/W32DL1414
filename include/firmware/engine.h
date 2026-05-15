#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "shared/W32DL1414_prtcl.h"

struct engine_job_t {
    uint8_t status;
    uint8_t type;
    uint8_t offset;
    uint8_t arg0;
    uint8_t arg1;
    uint8_t progress;
    uint8_t total;
    uint8_t steps_per_tick;
    uint8_t settle_ticks;

};

struct engine_operation_t {
    uint8_t opcode;
    uint8_t status;
    uint8_t error_code;
};

struct engine_state_t {
    uint8_t status;
    engine_job_t job;
    engine_operation_t operation;
};

void engine_init(void);
void engine_reset(void);

uint8_t engine_get_status(void);
engine_state_t* engine_get_state(void);

void engine_preserve_state_snapshot(void);
const engine_state_t* engine_get_preserved_state(void);

void engine_set_ready(void);
void engine_set_busy(void);
void engine_set_error(void);

void engine_start_operation(uint8_t opcode);
void engine_finish_operation(bool successful, uint8_t error_code);

void engine_clear_job(void);
void engine_start_job(uint8_t job_type, uint8_t total);
void engine_set_job_args(uint8_t arg0, uint8_t arg1);
void engine_update_job_progress(uint8_t progress);
void engine_finish_job(bool successful, uint8_t error_code);

bool engine_is_busy(void);
bool engine_has_job(void);

void engine_response_begin(uint8_t* output_len, uint8_t* output_buf, bool successful, uint8_t error_code);
bool engine_response_push_u8(uint8_t* output_len, uint8_t* output_buf, uint8_t value);
bool engine_response_push_u16le(uint8_t* output_len, uint8_t* output_buf, uint16_t value);
bool engine_response_push_bytes(uint8_t* output_len, uint8_t* output_buf, const uint8_t* data, uint8_t len);
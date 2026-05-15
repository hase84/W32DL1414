#pragma once

#include "firmware/jobs.h"
#include "firmware/dispatch.h"

#include <stdbool.h>
#include <stdint.h>


#include "firmware/engine.h"
#include "firmware/vram.h"
#include "shared/W32DL1414_prtcl.h"
#include "firmware/board.h"


static bool job_tick_flush_vram(engine_state_t* state)
{
    if (state == nullptr) {
        engine_finish_job(false, W32DL1414_ERROR_INTERNAL);
        return false;
    }

    if (state->job.arg1 > W32DL1414_DISPLAY_SIZE) {
        engine_finish_job(false, W32DL1414_ERROR_OUT_OF_RANGE);
        return false;
    }

    if (state->job.offset >= state->job.arg1) {
        engine_finish_job(true, W32DL1414_ERROR_NONE);
        return true;
    }

    write_to_display(state->job.offset, vram[state->job.offset]);
    vram_clear_dirty(state->job.offset);
    ++state->job.offset;

    if (state->job.total > 0) {
        const uint16_t numerator = (uint16_t)state->job.offset * 100U;
        engine_update_job_progress((uint8_t)(numerator / state->job.total));
    } else {
        engine_update_job_progress(100);
    }

    if (state->job.offset >= state->job.arg1) {
        engine_finish_job(true, W32DL1414_ERROR_NONE);
        return true;
    }

    return false;
}

static bool job_tick_clear_vram(engine_state_t* state)
{
    if (state == nullptr) {
        engine_finish_job(false, W32DL1414_ERROR_INTERNAL);
        return false;
    }

    if (state->job.arg1 > W32DL1414_DISPLAY_SIZE) {
        engine_finish_job(false, W32DL1414_ERROR_OUT_OF_RANGE);
        return false;
    }

    if (state->job.offset >= state->job.arg1) {
        engine_finish_job(true, W32DL1414_ERROR_NONE);
        return true;
    }

    const uint8_t fill = state->job.arg0;

    vram_write(state->job.offset, fill);
    ++state->job.offset;

    if (state->job.total > 0) {
        const uint16_t numerator = (uint16_t)state->job.offset * 100U;
        engine_update_job_progress((uint8_t)(numerator / state->job.total));
    } else {
        engine_update_job_progress(100);
    }

    if (state->job.offset >= state->job.arg1) {
        engine_finish_job(true, W32DL1414_ERROR_NONE);
        return true;
    }

    return false;
}

void jobs_execute_tick(void)
{
    engine_state_t* state = engine_get_state();

    if (state == nullptr) {
        return;
    }

    if (!engine_has_job()) {
        return;
    }

    const job_handler_entry_t* handler = find_job_handler(state->job.type);
    if (handler == nullptr || handler->tick == nullptr) {
        engine_finish_job(false, W32DL1414_ERROR_NOT_IMPLEMENTED);
        return;
    }

    uint8_t steps = (state->job.steps_per_tick == 0) ? 1 : state->job.steps_per_tick;
    bool completed = false;

    while (steps-- > 0 && engine_has_job()) {
        completed = handler->tick(state);
        if (completed) {
            break;
        }
    }

    if (!completed) {
        return;
    }

    if (state->job.settle_ticks > 0) {
        --state->job.settle_ticks;
        return;
    }

    engine_finish_job(true, W32DL1414_ERROR_NONE);
}
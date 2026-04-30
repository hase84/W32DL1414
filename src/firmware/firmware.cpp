#include "shared/W32DL1414_prtcl.h"
#include "firmware/firmware.h"
#include "firmware/engine.h"
#include "firmware/opcodes.h"
#include "firmware/jobs.h"
#include "firmware/dispatch.h"

extern "C" {
#include "usitwislave.h"
}

void jobs_execute_tick(void);



void request_handler(volatile uint8_t input_len, const uint8_t* input_buf,
                     uint8_t* output_len, uint8_t* output_buf)
{
    if (output_len == nullptr || output_buf == nullptr) {
        return;
    }

    *output_len = 0;

    if (input_buf == nullptr || input_len == 0) {
        return;
    }

    const uint8_t opcode = input_buf[0];
    const opcode_handler_entry_t* entry = find_opcode_handler(opcode);

    if (entry == nullptr || entry->handler == nullptr) {
        engine_start_operation(opcode);
        engine_response_begin(output_len, output_buf, false, W32DL1414_ERROR_INVALID_OPCODE);
        engine_finish_operation(false, W32DL1414_ERROR_INVALID_OPCODE);
        return;
    }

    if (engine_has_job() && (entry->flags & OPCODE_FLAG_ALLOWED_DURING_JOB) == 0) {
        engine_start_operation(opcode);
        engine_response_begin(output_len, output_buf, false, W32DL1414_ERROR_BUSY);
        engine_finish_operation(false, W32DL1414_ERROR_BUSY);
        return;
    }

    entry->handler(input_len, input_buf, output_len, output_buf);
}

void idle(void)
{
    jobs_execute_tick();
}
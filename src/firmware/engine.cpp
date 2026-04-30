#include "firmware/engine.h"

static engine_state_t engine;

static void reset_job_internal(void)
{
    engine.job.status = W32DL1414_JOB_STATUS_NONE;
    engine.job.type = W32DL1414_JOB_TYPE_NONE;
    engine.job.offset = 0;
    engine.job.arg0 = 0;
    engine.job.arg1 = 0;
    engine.job.progress = 0;
    engine.job.total = 0;
}

static void reset_operation_internal(void)
{
    engine.operation.opcode = 0;
    engine.operation.status = W32DL1414_OPERATION_STATUS_IDLE;
    engine.operation.error_code = W32DL1414_ERROR_NONE;
}

void engine_init(void)
{
    engine.status = W32DL1414_ENGINE_STATUS_READY;
    reset_job_internal();
    reset_operation_internal();
}

void engine_reset(void)
{
    engine_init();
}

uint8_t engine_get_status(void)
{
    return engine.status;
}

engine_state_t* engine_get_state(void)
{
    return &engine;
}

void engine_set_ready(void)
{
    engine.status = W32DL1414_ENGINE_STATUS_READY;
}

void engine_set_busy(void)
{
    engine.status = W32DL1414_ENGINE_STATUS_BUSY;
}

void engine_set_error(void)
{
    engine.status = W32DL1414_ENGINE_STATUS_ERROR;
}

void engine_start_operation(uint8_t opcode)
{
    engine.operation.opcode = opcode;
    engine.operation.status = W32DL1414_OPERATION_STATUS_RUNNING;
    engine.operation.error_code = W32DL1414_ERROR_NONE;
}

void engine_finish_operation(bool successful, uint8_t error_code)
{
    if (successful) {
        engine.operation.status = W32DL1414_OPERATION_STATUS_OK;
        engine.operation.error_code = W32DL1414_ERROR_NONE;
        engine.status = W32DL1414_ENGINE_STATUS_READY;
    } else {
        engine.operation.status = W32DL1414_OPERATION_STATUS_FAILED;
        engine.operation.error_code = error_code;
        engine.status = W32DL1414_ENGINE_STATUS_ERROR;
    }
}

void engine_clear_job(void)
{
    reset_job_internal();
    engine.status = W32DL1414_ENGINE_STATUS_READY;
}

void engine_start_job(uint8_t job_type, uint8_t total)
{
    reset_job_internal();
    engine.job.status = W32DL1414_JOB_STATUS_PENDING;
    engine.job.type = job_type;
    engine.job.offset = 0;
    engine.job.arg0 = 0;
    engine.job.arg1 = 0;
    engine.job.progress = 0;
    engine.job.total = total;
    engine.status = W32DL1414_ENGINE_STATUS_BUSY;
}

void engine_set_job_args(uint8_t arg0, uint8_t arg1)
{
    engine.job.arg0 = arg0;
    engine.job.arg1 = arg1;
}

void engine_update_job_progress(uint8_t progress)
{
    engine.job.status = W32DL1414_JOB_STATUS_RUNNING;
    engine.job.progress = progress;
}

void engine_finish_job(bool successful, uint8_t error_code)
{
    if (successful) {
        engine.job.status = W32DL1414_JOB_STATUS_DONE;
        engine.operation.status = W32DL1414_OPERATION_STATUS_OK;
        engine.operation.error_code = W32DL1414_ERROR_NONE;
        engine.status = W32DL1414_ENGINE_STATUS_READY;
    } else {
        engine.job.status = W32DL1414_JOB_STATUS_FAILED;
        engine.operation.status = W32DL1414_OPERATION_STATUS_FAILED;
        engine.operation.error_code = error_code;
        engine.status = W32DL1414_ENGINE_STATUS_ERROR;
    }
}


bool engine_is_busy(void)
{
    return engine.status == W32DL1414_ENGINE_STATUS_BUSY;
}

bool engine_has_job(void)
{
    return engine.job.status == W32DL1414_JOB_STATUS_PENDING ||
           engine.job.status == W32DL1414_JOB_STATUS_RUNNING;
}

void engine_response_begin(uint8_t* output_len,
                           uint8_t* output_buf,
                           bool successful,
                           uint8_t error_code)
{
    if (output_len == nullptr || output_buf == nullptr) {
        return;
    }

    if (successful) {
        output_buf[0] = W32DL1414_RESULT_OK;
        *output_len = 1;
    } else {
        output_buf[0] = W32DL1414_RESULT_FAILED;
        output_buf[1] = error_code;
        *output_len = 2;
    }
}

bool engine_response_push_u8(uint8_t* output_len, uint8_t* output_buf, uint8_t value)
{
    if (output_len == nullptr || output_buf == nullptr) {
        return false;
    }

    if (*output_len >= W32DL1414_MAX_RESPONSE_LEN) {
        return false;
    }

    output_buf[*output_len] = value;
    ++(*output_len);
    return true;
}

bool engine_response_push_u16le(uint8_t* output_len, uint8_t* output_buf, uint16_t value)
{
    return engine_response_push_u8(output_len, output_buf, (uint8_t)(value & 0xFF)) &&
           engine_response_push_u8(output_len, output_buf, (uint8_t)((value >> 8) & 0xFF));
}

bool engine_response_push_bytes(uint8_t* output_len, uint8_t* output_buf, const uint8_t* data, uint8_t len)
{
    if (output_len == nullptr || output_buf == nullptr || data == nullptr) {
        return false;
    }

    for (uint8_t i = 0; i < len; ++i) {
        if (!engine_response_push_u8(output_len, output_buf, data[i])) {
            return false;
        }
    }

    return true;
}
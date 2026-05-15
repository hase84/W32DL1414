#include "firmware/dispatch.h"

#include "shared/W32DL1414_prtcl.h"
#include "firmware/jobs.h"


static const opcode_handler_entry_t opcode_handlers[] = {
//  { opcode,                           flags,                          min_input_len,  max_input_len, handler_fn                    },    
    { W32DL1414_OPCODE_PING,            OPCODE_FLAG_ALLOWED_DURING_JOB, 1,                          1, opcode_handle_ping            },
    { W32DL1414_OPCODE_GET_VERSION,     OPCODE_FLAG_ALLOWED_DURING_JOB, 1,                          1, opcode_handle_get_version     },
    { W32DL1414_OPCODE_GET_STATUS,      OPCODE_FLAG_ALLOWED_DURING_JOB, 1,                          1, opcode_handle_get_status      },
    { W32DL1414_OPCODE_GET_SYSTEM_INFO, OPCODE_FLAG_ALLOWED_DURING_JOB, 1,                          1, opcode_handle_get_system_info },
    { W32DL1414_OPCODE_WRITE_CHAR,      OPCODE_FLAG_NONE,               3,                          3, opcode_handle_write_char      },
    { W32DL1414_OPCODE_WRITE_BUF,       OPCODE_FLAG_NONE,               4, W32DL1414_MAX_TRANSFER_LEN, opcode_handle_write_buf       },
    { W32DL1414_OPCODE_FLUSH,           OPCODE_FLAG_NONE,               1,                          1, opcode_handle_flush           },
    { W32DL1414_OPCODE_CLEAR,           OPCODE_FLAG_NONE,               2,                          2, opcode_handle_clear           },
};

static const job_handler_entry_t job_handlers[] = {
    { W32DL1414_JOB_TYPE_FLUSH_VRAM, 4, 1, job_tick_flush_vram },
    { W32DL1414_JOB_TYPE_CLEAR_VRAM, 4, 0, job_tick_clear_vram },
};


const opcode_handler_entry_t* find_opcode_handler(uint8_t opcode)
{
    const uint8_t count = sizeof(opcode_handlers) / sizeof(opcode_handlers[0]);

    for (uint8_t i = 0; i < count; ++i) {
        if (opcode_handlers[i].opcode == opcode) {
            return &opcode_handlers[i];
        }
    }

    return nullptr;
}


const job_handler_entry_t* find_job_handler(uint8_t job_type)
{
    const uint8_t count = sizeof(job_handlers) / sizeof(job_handlers[0]);

    for (uint8_t i = 0; i < count; ++i) {
        if (job_handlers[i].job_type == job_type) {
            return &job_handlers[i];
        }
    }

    return nullptr;
}
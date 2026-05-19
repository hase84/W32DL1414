#include <avr/pgmspace.h>

#include "firmware/dispatch.h"

#include "firmware/opcodes.h"
#include "shared/W32DL1414_prtcl.h"
#include "firmware/jobs.h"




static const opcode_handler_entry_t opcode_handlers[] PROGMEM = {
//  { opcode,                                   flags,                          min_input_len,  max_input_len, handler_fn                            },    
    { W32DL1414_OPCODE_PING,                    OPCODE_FLAG_ALLOWED_DURING_JOB,  1,                          1, opcode_handle_ping                   },
    { W32DL1414_OPCODE_GET_VERSION,             OPCODE_FLAG_ALLOWED_DURING_JOB,  1,                          1, opcode_handle_get_version            },
    { W32DL1414_OPCODE_GET_STATUS,              OPCODE_FLAG_ALLOWED_DURING_JOB,  1,                          1, opcode_handle_get_status             },
    { W32DL1414_OPCODE_GET_SYSTEM_INFO,         OPCODE_FLAG_ALLOWED_DURING_JOB,  1,                          1, opcode_handle_get_system_info        },
    { W32DL1414_OPCODE_WRITE_CHAR,              OPCODE_FLAG_NONE,                3,                          3, opcode_handle_write_char             },
    { W32DL1414_OPCODE_WRITE_BUF,               OPCODE_FLAG_NONE,                4, W32DL1414_MAX_TRANSFER_LEN, opcode_handle_write_buf              },
    { W32DL1414_OPCODE_READ_BUF,                OPCODE_FLAG_NONE,                3,                          3, opcode_handle_read_buf               },
    { W32DL1414_OPCODE_FLUSH,                   OPCODE_FLAG_NONE,                1,                          1, opcode_handle_flush                  },
    { W32DL1414_OPCODE_CLEAR,                   OPCODE_FLAG_NONE,                2,                          2, opcode_handle_clear                  },
    { W32DL1414_OPCODE_SET_DISPLAY_CONFIG,      OPCODE_FLAG_NONE,                4,                          4,  opcode_handle_set_display_config    },
    { W32DL1414_OPCODE_GET_DISPLAY_CONFIG,      OPCODE_FLAG_NONE,                1,                          1,  opcode_handle_get_display_config    },
    { W32DL1414_OPCODE_SET_CURSOR_CONFIG,       OPCODE_FLAG_NONE,                8,                          8,  opcode_handle_set_cursor_config     },
    { W32DL1414_OPCODE_GET_CURSOR_CONFIG,       OPCODE_FLAG_NONE,                1,                          1,  opcode_handle_get_cursor_config     },
    { W32DL1414_OPCODE_SOFT_RESET,              OPCODE_FLAG_ALLOWED_DURING_JOB,  1,                          1,  opcode_handle_soft_reset            },
    { W32DL1414_OPCODE_HARD_RESET,              OPCODE_FLAG_ALLOWED_DURING_JOB,  1,                          1,  opcode_handle_hard_reset            },
    { W32DL1414_OPCODE_READ_DIP,                OPCODE_FLAG_ALLOWED_DURING_JOB,  1,                          1,  opcode_handle_read_dip              }

};

static const job_handler_entry_t job_handlers[] PROGMEM = {
//  { job_type, steps per tick, settle ticks, tick_fn },    
    { W32DL1414_JOB_TYPE_FLUSH_VRAM, 4, 1, job_tick_flush_vram },
    { W32DL1414_JOB_TYPE_CLEAR_VRAM, 4, 0, job_tick_clear_vram },
    { W32DL1414_JOB_TYPE_SCROLL_VRAM, 4, 0, job_tick_scroll_vram }
};


bool find_opcode_handler(uint8_t opcode, opcode_handler_entry_t* out_entry)
{
    if (out_entry == nullptr) {
        return false;
    }

    const uint8_t count = sizeof(opcode_handlers) / sizeof(opcode_handlers[0]);

    for (uint8_t i = 0; i < count; ++i) {
        opcode_handler_entry_t tmp;
        memcpy_P(&tmp, &opcode_handlers[i], sizeof(tmp));

        if (tmp.opcode == opcode) {
            *out_entry = tmp; // in den Caller kopieren
            return true;
        }
    }

    return false;
}

bool find_job_handler(uint8_t job_type, job_handler_entry_t* out_entry)
{
    if (out_entry == nullptr) {
        return false;
    }

    const uint8_t count = sizeof(job_handlers) / sizeof(job_handlers[0]);

    for (uint8_t i = 0; i < count; ++i) {
        job_handler_entry_t tmp;
        memcpy_P(&tmp, &job_handlers[i], sizeof(tmp));

        if (tmp.job_type == job_type) {
            *out_entry = tmp;
            return true;
        }
    }

    return false;
}

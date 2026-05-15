#pragma once

/* =========================================================
 * display geometry
 * ========================================================= */
#define W32DL1414_DISPLAY_SIZE                         128

/* =========================================================
 * protocol limits
 * ========================================================= */
#define W32DL1414_MAX_TRANSFER_LEN                     32
#define W32DL1414_MAX_RESPONSE_LEN                     32


/* =========================================================
 * opcodes
 * ========================================================= */
#define W32DL1414_OPCODE_PING                          0x01
#define W32DL1414_OPCODE_GET_VERSION                   0x02
#define W32DL1414_OPCODE_GET_STATUS                    0x03

#define W32DL1414_OPCODE_WRITE_CHAR                    0x20
#define W32DL1414_OPCODE_WRITE_BUF                     0x21
#define W32DL1414_OPCODE_FLUSH                         0x22
#define W32DL1414_OPCODE_CLEAR                         0x23

#define W32DL1414_OPCODE_SET_CURSOR_ENABLE             0x42
#define W32DL1414_OPCODE_GET_CURSOR_ENABLE             0x43
#define W32DL1414_OPCODE_SET_CURSOR_VISIBLE            0x44
#define W32DL1414_OPCODE_GET_CURSOR_VISIBLE            0x45
#define W32DL1414_OPCODE_SET_CURSOR_POS                0x46
#define W32DL1414_OPCODE_GET_CURSOR_POS                0x47
#define W32DL1414_OPCODE_SET_CURSOR_CHAR               0x48
#define W32DL1414_OPCODE_GET_CURSOR_CHAR               0x49
#define W32DL1414_OPCODE_SET_CURSOR_BLINK_FREQ         0x4A
#define W32DL1414_OPCODE_GET_CURSOR_BLINK_FREQ         0x4B

#define W32DL1414_OPCODE_SET_REFRESH_MODE              0x60
#define W32DL1414_OPCODE_GET_REFRESH_MODE              0x61
#define W32DL1414_OPCODE_SET_SCROLL_MODE               0x62
#define W32DL1414_OPCODE_GET_SCROLL_MODE               0x63

#define W32DL1414_OPCODE_GET_SYSTEM_INFO               0x80
#define W32DL1414_OPCODE_SOFT_RESET                    0x81
#define W32DL1414_OPCODE_HARD_RESET                    0x82
#define W32DL1414_OPCODE_READ_DIP                      0x83


/* =========================================================
 * result / errors
 * ========================================================= */
#define W32DL1414_RESULT_OK                           0x01
#define W32DL1414_RESULT_FAILED                       0xFF

#define W32DL1414_ERROR_NONE                          0x00
#define W32DL1414_ERROR_INVALID_OPCODE                0xE0
#define W32DL1414_ERROR_INVALID_ARGUMENT              0xE1
#define W32DL1414_ERROR_OUT_OF_RANGE                  0xE2
#define W32DL1414_ERROR_BUSY                          0xE3
#define W32DL1414_ERROR_INTERNAL                      0xE4
#define W32DL1414_ERROR_BUFFER_OVERFLOW               0xE5
#define W32DL1414_ERROR_NOT_IMPLEMENTED               0xE6
#define W32DL1414_ERROR_INVALID_LENGTH                0xE7


/* =========================================================
 * engine status
 * ========================================================= */
#define W32DL1414_ENGINE_STATUS_READY                 0x00
#define W32DL1414_ENGINE_STATUS_BUSY                  0x01
#define W32DL1414_ENGINE_STATUS_ERROR                 0x02


/* =========================================================
 * job status
 * ========================================================= */
#define W32DL1414_JOB_STATUS_NONE                     0x00
#define W32DL1414_JOB_STATUS_PENDING                  0x01
#define W32DL1414_JOB_STATUS_RUNNING                  0x02
#define W32DL1414_JOB_STATUS_DONE                     0x03
#define W32DL1414_JOB_STATUS_FAILED                   0x04


/* =========================================================
 * job types
 * ========================================================= */
#define W32DL1414_JOB_TYPE_NONE                       0x00
#define W32DL1414_JOB_TYPE_FLUSH_VRAM                 0x01
#define W32DL1414_JOB_TYPE_CLEAR_VRAM                 0x02
#define W32DL1414_JOB_TYPE_SCROLL_LEFT                0x03
#define W32DL1414_JOB_TYPE_SCROLL_RIGHT               0x04
#define W32DL1414_JOB_TYPE_WRITE_BUFFER               0x05


/* =========================================================
 * operation status
 * ========================================================= */
#define W32DL1414_OPERATION_STATUS_IDLE               0x00
#define W32DL1414_OPERATION_STATUS_RUNNING            0x01
#define W32DL1414_OPERATION_STATUS_OK                 0x02
#define W32DL1414_OPERATION_STATUS_FAILED             0x03


/* =========================================================
 * board ids for protocol payload
 * ========================================================= */
#define W32DL1414_BOARD_ID_UNKNOWN                    0x00
#define W32DL1414_BOARD_ID_ATTINY44                   0x01
#define W32DL1414_BOARD_ID_ATTINY84                   0x02


/* =========================================================
 * cursor / text control
 * ========================================================= */
#define W32DL1414_CURSOR_INVALID                      0xFF
#define W32DL1414_FILL_CHAR                           0x20
#define W32DL1414_ASCII_TAB                           0x09
#define W32DL1414_ASCII_LF                            0x0A
#define W32DL1414_ASCII_CR                            0x0D

/* =========================================================
 * DISPLAY REFRESH MODES / AUTO vs MANUAL FLUSH
 * ========================================================= */
#define W32DL1414_REFRESH_MODE_AUTO                    0x00
#define W32DL1414_REFRESH_MODE_MANUAL                  0x01

/* =========================================================
 * SCROLL MODES
 * ========================================================= */
#define W32DL1414_SCROLL_MODE_OFF                      0x00
#define W32DL1414_SCROLL_MODE_LEFT                     0x01
#define W32DL1414_SCROLL_MODE_UP                       0x02

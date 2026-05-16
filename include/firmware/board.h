#pragma once

#include <stdint.h>
#include <Arduino.h>


#ifndef CLOCK_SPEED
  #error "CLOCK_SPEED is not defined. Set build_flags = -DCLOCK_SPEED=${board_build.f_cpu}UL"
#endif

#ifndef F_CPU
  #error "F_CPU is not defined. Set board_build.f_cpu in platformio.ini"
#endif

#if CLOCK_SPEED != F_CPU
  #error "CLOCK_SPEED and F_CPU differ. Keep both values identical."
#endif

#if defined(ARDUINO_AVR_ATTINY44) || defined(__AVR_ATtiny44__) || defined(__AVR_ATtiny44A__)
  #define W32DL1414_BOARD_TEXT "attiny44"
  #define W32DL1414_BOARD_ATTINY44 1
  #define W32DL1414_BOARD_ATTINY84 0
  #define W32DL1414_RAM_TOTAL_BYTES 256U

#elif defined(ARDUINO_AVR_ATTINY84) || defined(__AVR_ATtiny84__) || defined(__AVR_ATtiny84A__)
  #define W32DL1414_BOARD_TEXT "attiny84"
  #define W32DL1414_BOARD_ATTINY44 0
  #define W32DL1414_BOARD_ATTINY84 1
  #define W32DL1414_RAM_TOTAL_BYTES 512U

#else
  #pragma message ("Firmware version " FW_VERSION_TEXT ": unsupported board selected")
  #error "Board not supported in this firmware. Allowed boards: attiny44, attiny84."
#endif

#define W32DL1414_CLOCK_SPEED_HZ ((uint32_t)(CLOCK_SPEED))
#define W32DL1414_CPU_HZ         ((uint32_t)(F_CPU))

/* =========================================================
 * dl1414 command offsets
 * ========================================================= */
#define CMD_OFFSET_ROW              0x01
#define CMD_OFFSET_COL              0x04
#define CMD_OFFSET_DIGIT            0x07
#define CMD_OFFSET_ASCII            0x09

/* =========================================================
 * PIN definitions 74hc595
 * ========================================================= */
#define SER_OUT                     PA0
#define SRCLK                       PA1
#define OE                          PA2
#define RCLK                        PA3

/* =========================================================
 * PIN definitions 74hc595
 * ========================================================= */
#define DIP_ADDR_BIT0               PB0
#define DIP_ADDR_BIT1               PB1
#define DIP_ADDR_BIT2               PB2

uint16_t sys_calc_free_ram_bytes(void);
void write_to_display(uint8_t position, uint8_t ascii);
void schedule_hardware_reset(uint8_t timeout_ms);
uint8_t read_dip_switches(void);



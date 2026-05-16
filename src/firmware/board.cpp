#include <Arduino.h>
#include <avr/interrupt.h>
#include <avr/wdt.h>

#include "firmware/board.h"
#include "shared/W32DL1414_prtcl.h"

uint16_t sys_calc_free_ram_bytes(void)
{
    extern int __heap_start;
    extern int* __brkval;
    int v;

    return (uint16_t)((int)&v - (__brkval == 0 ? (int)&__heap_start : (int)__brkval));
}

static void shift_command_word(uint16_t command)
{
    PORTA &= ~(1 << RCLK);
    PORTA &= ~(1 << SRCLK);
    delayMicroseconds(1);   //8

    for (uint8_t i = 0; i < 16; i++) {
        if (command & (1 << i)) {
            PORTA |= (1 << SER_OUT);
        } else {
            PORTA &= ~(1 << SER_OUT);
        }

        delayMicroseconds(1);  //   4     
        PORTA |= (1 << SRCLK);
        delayMicroseconds(1);   //6
        PORTA &= ~(1 << SRCLK);
        delayMicroseconds(1);   //4
    }

    PORTA &= ~(1 << SER_OUT);
    delayMicroseconds(1);   //20
    PORTA |= (1 << RCLK);
    delayMicroseconds(1);  //20  
    PORTA &= ~(1 << RCLK);
    delayMicroseconds(1);  //30
}

static void write_digit_to_display(uint8_t position, uint8_t ascii)
{
    const uint8_t row = position / 32;
    const uint8_t column = (position % 32) / 4;
    const uint8_t digit = 3 - (position % 4);

    const uint16_t data_and_address =
        ((uint16_t)(ascii & 0x7F) << CMD_OFFSET_ASCII) |
        ((uint16_t)(digit & 0x03) << CMD_OFFSET_DIGIT);

    const uint16_t write_pulse =
        ((uint16_t)(CMD_OFFSET_ROW << row) & 0x0F) |
        ((uint16_t)(column & 0x07) << CMD_OFFSET_COL);

    shift_command_word(0x0000);
//    delayMicroseconds(1);  //10

    shift_command_word(data_and_address);
//    delayMicroseconds(5);   //10

    shift_command_word(data_and_address | write_pulse);
//    delayMicroseconds(5);  //10

    shift_command_word(data_and_address);
//    delayMicroseconds(5);  //10

    shift_command_word(0x0000);
//    delayMicroseconds(5);  //10
}

void write_to_display(uint8_t position, uint8_t ascii)
{
    if (position >= W32DL1414_DISPLAY_SIZE) {
        return;
    }

    write_digit_to_display(position, ascii);
}


void schedule_hardware_reset(uint8_t timeout_ms)
{
    cli();
    wdt_reset();
    wdt_enable(timeout_ms);

    for (;;)
    {
    }
}


uint8_t read_dip_switches(void)
{
    return (~PINB) & 0x07; 
}

#include <Arduino.h>
#include <avr/interrupt.h>
#include <avr/wdt.h>
#include <stdint.h>
#include <sys/types.h>

#include "firmware/board.h"
#include "firmware/engine.h"
#include "firmware/firmware.h"
#include "firmware/vram.h"
#include "firmware/version.h"
#include "shared/W32DL1414_prtcl.h"

extern "C" {
#include "usitwislave.h"
}

static void board_pins_init(void)
{
    DDRA |= (1 << SER_OUT) | (1 << SRCLK) | (1 << OE) | (1 << RCLK);
    PORTA &= ~((1 << SER_OUT) | (1 << SRCLK) | (1 << RCLK));
    PORTA &= ~(1 << OE);
}


static const char startup_message[] PROGMEM =\
     "W32DL1414 32X4 I2C ASCII DISPLAY"\
     "FIRMWARE #.#.# A. WUTHCKE 5/2026"\
     "ATTINY#4 #MHZ,###/### BYTES FREE"\
     "PROTOCOL READY @ ADDRESS 0X##...";

void display_startup_message(void)
{
    uint8_t character;
    uint8_t free_ram = sys_calc_free_ram_bytes(); 
    uint8_t i2c_address = W32DL1414_I2C_BASE_ADDRESS | read_dip_switches();
    for (uint8_t i = 0; i < sizeof(startup_message) - 1 && i < W32DL1414_DISPLAY_SIZE; ++i) {
        character = pgm_read_byte_near(startup_message + i);
            write_to_display(i, character);
    }
    write_to_display(41, '0' + (FW_VER_MAJOR % 10));
    write_to_display(43, '0' + (FW_VER_MINOR % 10));    
    write_to_display(45, '0' + (FW_VER_PATCH % 10));
    write_to_display(70, W32DL1414_BOARD_ATTINY44==1 ? '4' : (W32DL1414_BOARD_ATTINY84==1 ? '8' : '?'));
    write_to_display(73, '0' + (uint8_t)(W32DL1414_CLOCK_SPEED_HZ / 1000000UL));   
    write_to_display(78, '0' + (uint8_t)(free_ram / 100));   
    write_to_display(79, '0' + (uint8_t)((free_ram / 10) % 10));   
    write_to_display(80, '0' + (uint8_t)(free_ram % 10));   
    write_to_display(82, '0' + (uint8_t)(W32DL1414_RAM_TOTAL_BYTES / 100));   
    write_to_display(83, '0' + (uint8_t)((W32DL1414_RAM_TOTAL_BYTES / 10) % 10));   
    write_to_display(84, '0' + (uint8_t)(W32DL1414_RAM_TOTAL_BYTES % 10));   
    write_to_display(123, (uint8_t)((i2c_address>>4) & 0x0F)<10 ? '0'+((i2c_address>>4) & 0x0F) : 'A'+((i2c_address>>4) & 0x0F)-10);   
    write_to_display(124, (uint8_t)((i2c_address & 0x0F)<10 ? '0'+(i2c_address & 0x0F) : 'A'+(i2c_address & 0x0F)-10));    
}

void setup()
{
    // Disable watchdog if it was enabled by a previous faulty firmware
    MCUSR &= ~(1 << WDRF);
    wdt_disable();

    // Initialize hardware and subsystems
    board_pins_init();
    vram_init();
    engine_init();

    display_startup_message();

    usi_twi_slave(0x40, 32, request_handler, idle);
}

void loop()
{
}
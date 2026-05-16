#include <Arduino.h>
#include <avr/interrupt.h>
#include <avr/wdt.h>

#include "firmware/board.h"
#include "firmware/engine.h"
#include "firmware/firmware.h"
#include "firmware/vram.h"

extern "C" {
#include "usitwislave.h"
}

static void board_pins_init(void)
{
    DDRA |= (1 << SER_OUT) | (1 << SRCLK) | (1 << OE) | (1 << RCLK);
    PORTA &= ~((1 << SER_OUT) | (1 << SRCLK) | (1 << RCLK));
    PORTA &= ~(1 << OE);
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

    write_to_display(0, 'W');
    write_to_display(1, '3');
    write_to_display(2, '2');
    write_to_display(3, 'D');
    write_to_display(4, 'L');
    write_to_display(5, '1');
    write_to_display(6, '4');
    write_to_display(7, '1');
    write_to_display(8, '4');
    write_to_display(10, 'R');
    write_to_display(11, 'E');
    write_to_display(12, 'A');
    write_to_display(13, 'D');
    write_to_display(14, 'Y');
    write_to_display(16, '@');
    write_to_display(18, '4');
    write_to_display(19, '0' + read_dip_switches());
    write_to_display(20, 'H');

    usi_twi_slave(0x40, 32, request_handler, idle);
}

void loop()
{
}
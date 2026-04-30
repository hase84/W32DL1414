#include <Arduino.h>

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
    board_pins_init();
    vram_init();
    engine_init();

    vram_write(0, 'T');
    vram_write(1, 'E');
    vram_write(2, 'S');
    vram_write(3, 'T');
    vram_write(4, ' ');
    vram_write(5, 'O');
    vram_write(6, 'K');

    usi_twi_slave(0x40, 32, request_handler, idle);
}

void loop()
{
}
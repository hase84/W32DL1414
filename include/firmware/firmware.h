#pragma once

#include <stdint.h>



void request_handler(volatile uint8_t input_len, const uint8_t* input_buf,
                     uint8_t* output_len, uint8_t* output_buf);

void idle(void);
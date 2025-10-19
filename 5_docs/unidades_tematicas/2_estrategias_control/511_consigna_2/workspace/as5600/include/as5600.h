#ifndef AS5600_H_
#define AS5600_H_

#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"

#define AS5600_ADDRESS 0x36
#define AS5600_DIR_PIN  6

#define AS5600_ANGLE_REG_HIGH 0x0E
#define AS5600_ANGLE_REG_LOW  0x0F
#define AS5600_STATUS_REG 0x0B

#define AS5600_AGC_REG 0x1A
#define AS5600_MAG_REG_HIGH 0x1B

typedef struct {
    uint8_t mh;
    uint8_t ml;
    uint8_t md;
    uint8_t valid;
} as5600_status_t;

void init_as5600();
void set_as5600_dir(uint8_t dir);
as5600_status_t get_as5600_status(i2c_inst_t *i2c);
uint16_t get_as5600_angle(i2c_inst_t *i2c);
uint16_t get_angle_position(i2c_inst_t *i2c);

#endif
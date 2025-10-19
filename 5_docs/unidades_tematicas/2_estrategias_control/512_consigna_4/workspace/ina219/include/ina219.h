#ifndef _INA219_H_
#define _INA219_H_

#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "math.h"

#define INA219_I2C_ADDR         0x40
#define INA219_REG_CONFIG       0x00
#define INA219_REG_SHUNTVOLTAGE 0x01
#define INA219_REG_BUSVOLTAGE   0x02
#define INA219_REG_POWER        0x03
#define INA219_REG_CURRENT      0x04
#define INA219_REG_CALIBRATION  0x05

#define TIMEOUT_US              10000
#define INA219_ADJ              0.8f

typedef enum {
    INA219_OK,
    INA219_TIMEOUT              = -1,
    INA219_INVALID_PARAM        = -2,
    INA219_INIT_ERROR           = -3,
    INA219_CALIBRATION_ERROR    = -4,
} ina219_status_t;

typedef enum {
    INA219_GAIN_1_40MV   = 0U,
    INA219_GAIN_2_80MV   = 1U,
    INA219_GAIN_4_160MV  = 2U,
    INA219_GAIN_8_320MV  = 3U
} ina219_gain_t;

typedef struct {
    i2c_inst_t *i2c;
    uint8_t addr;
    float shunt_resistor_value;
    float max_expected_amps;
    uint8_t gain;
} ina219_t;

typedef struct {
    float voltage_v;  // Bus voltage in volts
    float current_a;  // Current in amperes
    float power_w;    // Power in watts
    float shunt_voltage_v; // Shunt voltage in volts
} ina219_data_t;


typedef struct {
    ina219_data_t *data;
    ina219_t ina219;
} ina219_context_t;

// Prototipos

static inline ina219_t ina219_get_default_config(void) {
    return (ina219_t) {
        .i2c = i2c0,
        .addr = INA219_I2C_ADDR,
        .shunt_resistor_value = 0.1f,
        .max_expected_amps = 3.2f,
        .gain = INA219_GAIN_8_320MV // Default gain
    };
}

ina219_status_t ina219_init_and_calibrate(ina219_t ina219);
ina219_status_t ina219_read_data(ina219_t ina219, ina219_data_t *data);
ina219_status_t ina219_read_voltage(ina219_t ina219, float *voltage);
ina219_status_t ina219_read_shunt_voltage(ina219_t ina219, float *shunt_voltage);
ina219_status_t ina219_read_current(ina219_t ina219, float *current);
ina219_status_t ina219_read_power(ina219_t ina219, float *power);

void ina219_init_rtos(void * context_ptr);
void ina219_get_data_rtos(void * context_ptr);

#endif // INA219_H
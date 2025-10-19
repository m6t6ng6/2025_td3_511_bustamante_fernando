// i2c_shared.h
#ifndef I2C_SHARED_H
#define I2C_SHARED_H

#include <stdint.h>

typedef enum {
    RECEIVE,
    TRANSMIT
} i2c_op_t;

typedef struct {
    uint16_t device_id;
    uint8_t reg;
    i2c_op_t type;
    uint8_t n_bytes;
    uint8_t *data;
} i2cQueue_t;

#endif // I2C_SHARED_H

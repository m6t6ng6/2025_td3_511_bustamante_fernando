#include "as5600.h"

void init_as5600() {
    gpio_init(AS5600_DIR_PIN);
    gpio_set_dir(AS5600_DIR_PIN, GPIO_OUT);
    gpio_put(AS5600_DIR_PIN, 0);
}

void set_as5600_dir(uint8_t dir) {
    gpio_put(AS5600_DIR_PIN, dir);
}

as5600_status_t get_as5600_status(i2c_inst_t *i2c) {
    as5600_status_t status;
    uint8_t buf;
    i2c_write_blocking(i2c, AS5600_ADDRESS, (uint8_t[]){AS5600_STATUS_REG}, 1, true);
    i2c_read_blocking(i2c, AS5600_ADDRESS, &buf, 1, false);
    buf = buf >> 3;
    status.mh  = buf & 0b001;
    status.ml = (buf & 0b010) >> 1;
    status.md = (buf & 0b100) >> 2;
    status.valid = (status.md & !status.ml & !status.mh) ? 1 : 0;

    return status;
}

uint16_t get_as5600_angle(i2c_inst_t *i2c) {
    uint8_t buffer[2];
    i2c_write_blocking(i2c, AS5600_ADDRESS, (uint8_t[]){AS5600_ANGLE_REG_HIGH}, 1, true);
    i2c_read_blocking(i2c, AS5600_ADDRESS, buffer, 2, false);
    uint16_t angle = ((buffer[0] & 0x0F) << 8 | buffer[1]);
    return angle;
}

uint16_t get_angle_position(i2c_inst_t *i2c) {
    uint16_t raw_angle = get_as5600_angle(i2c);  // Obtiene el valor bruto del ángulo (0-4095)
    uint16_t angle_in_degrees = (raw_angle * 360) / 4096;  // Escala el valor al rango 0-360°
    return (float) angle_in_degrees;
}
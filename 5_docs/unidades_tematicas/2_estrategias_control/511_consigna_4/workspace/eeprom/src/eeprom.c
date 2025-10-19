
#include "eeprom.h"
#include "pico/stdlib.h"
#include <string.h>

void eeprom_write_data(i2c_inst_t *i2c, uint16_t mem_address, const uint8_t *data, size_t len) {
    uint8_t buffer[EEPROM_PAGE_SIZE + 2];
    buffer[0] = (mem_address >> 8) & 0xFF;  // MSB
    buffer[1] = mem_address & 0xFF;         // LSB
    memcpy(&buffer[2], data, len);
    i2c_write_blocking(i2c, EEPROM_ADDR, buffer, len + 2, false);
    sleep_ms(5);  // Tiempo de escritura típico
}

void eeprom_read_data(i2c_inst_t *i2c, uint16_t mem_address, uint8_t *data, size_t len) {
    uint8_t addr_buf[2] = {
        (mem_address >> 8) & 0xFF,
        mem_address & 0xFF
    };
    i2c_write_blocking(i2c, EEPROM_ADDR, addr_buf, 2, true);
    i2c_read_blocking(i2c, EEPROM_ADDR, data, len, false);
}

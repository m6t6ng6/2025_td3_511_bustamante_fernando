
#ifndef EEPROM_H
#define EEPROM_H

#include "hardware/i2c.h"
#include <stdint.h>
#include <stddef.h>

#define EEPROM_ADDR 0x57
#define EEPROM_PAGE_SIZE 32

#define EEPROM_ADDR_SETPOINTS   0x0000
#define EEPROM_ADDR_ALARMAS     0x0400
#define EEPROM_ADDR_LECTURAS    0x0800
#define EEPROM_MAX_SIZE         0x1000  // 4096 bytes

#define EEPROM_SIZE_SETPOINTS   0x0400  // 1024 bytes
#define EEPROM_SIZE_ALARMAS     0x0400  // 1024 bytes
#define EEPROM_SIZE_LECTURAS    0x0800  // 2048 bytes


typedef enum {
    EEPROM_DATA_SETPOINT,
    EEPROM_DATA_ALARMA,
    EEPROM_DATA_LECTURA
} eeprom_data_type_t;   

typedef enum {
    ID_VMAX, ID_IMAX, ID_VMIN, ID_IMIN, ID_TIEMPO, ID_R1, ID_R2, ID_R3, ID_R4,
    ID_R5, ID_R6, ID_R7, ID_R8, ID_R9, ID_R10
} eeprom_data_id_t;

typedef enum {
    ALARMA_NONE = 0,
    ALARMA_VMAX = 1 << 0,
    ALARMA_IMAX = 1 << 1,
    ALARMA_VMIN = 1 << 2,
    ALARMA_IMIN = 1 << 3
} alarma_flag_t;

void eeprom_write_data(i2c_inst_t *i2c, uint16_t mem_address, const uint8_t *data, size_t len);
void eeprom_read_data(i2c_inst_t *i2c, uint16_t mem_address, uint8_t *data, size_t len);

#endif // EEPROM_H

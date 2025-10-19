#include "pico/stdlib.h"
#include <stdio.h>
#include "hardware/i2c.h"

#define DS1307_ADDRESS 0x68

typedef struct {
    uint8_t seconds;
    uint8_t minutes;
    uint8_t hours;
    uint8_t day;
    uint8_t date;
    uint8_t month;
    uint16_t year;  // El DS1307 no guarda el año exacto
} rtc_time_t;

typedef struct {
    rtc_time_t time;
    i2c_inst_t *i2c;
    uint8_t addr;
}ds1307_t;

void ds1307_init(ds1307_t *ds1307);
void ds1307_get_time(ds1307_t *ds1307);
void ds1307_set_time(ds1307_t *ds1307);
#ifndef _RTC_H_
#define _RTC_H_

#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"

// Comandos
#define RTC_SECONDS 0x00
#define RTC_MINUTES 0x01
#define RTC_HOURS 0x02
#define RTC_DAY 0x03
#define RTC_DATE 0x04
#define RTC_MONTH 0x05
#define RTC_YEAR 0x06
#define RTC_CONTROL 0x07

// Conversion de años
#define YEAR_BASE 2000

// Address
#define DS1307_ADDRESS 0x68

typedef struct {
    uint16_t year;
    uint8_t month;
    uint8_t date;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
} time_t;

void rtc_init(i2c_inst_t *i2c);
void rtc_set_time(time_t time);
time_t rtc_get_time(void);

#endif
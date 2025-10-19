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
#define RTC_CONTROL 0x0E
#define RTC_STATUS 0x0F

// Conversion de años
#define YEAR_BASE 2000

// Address
#define DS3231_ADDRESS 0x68

#ifndef TIMEOUT_US
#define TIMEOUT_US  10000
#endif

typedef struct {
    uint16_t year;
    uint8_t month;
    uint8_t date;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
} time_t;

void rtc_set_i2c(i2c_inst_t *i2c);
void rtc_init_rtos(void *i2c_param);
void rtc_set_time(time_t time);
uint8_t rtc_read_item(uint8_t reg);
time_t rtc_get_time(void);

void rtc_get_time_rtos(void *param);
void rtc_set_time_rtos(void *param);

#endif
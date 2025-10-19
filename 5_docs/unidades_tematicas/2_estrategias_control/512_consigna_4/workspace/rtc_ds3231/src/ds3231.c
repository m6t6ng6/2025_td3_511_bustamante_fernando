#include "ds3231.h"

// Puntero a I2C usado
static i2c_inst_t *rtc_i2c;

time_t default_time = {
    .year = 2025U,
    .month = 8,
    .date = 2,
    .day = 5,
    .hour = 0,
    .minute = 0,
    .second = 0
};

/**
 * @brief Manda un byte por I2C
 * @param val es el byte a mandar
*/
static int8_t i2c_write_reg(uint8_t reg, uint8_t val) {
    uint8_t datos[] = {reg, val};
    return i2c_write_timeout_us(rtc_i2c, DS3231_ADDRESS, datos, 2, false, TIMEOUT_US);
}

/**
 * @brief Lee multiples bytes por I2C
 * @param reg es el registro inicial
 * @param len cantidad de bytes a leer
 * @param buf buffer de datos
 */
static int8_t i2c_multiple_read(uint8_t reg, uint8_t len, uint8_t *buf) {
    i2c_write_timeout_us(rtc_i2c, DS3231_ADDRESS, &reg, 1, false, TIMEOUT_US);
    return (int8_t) i2c_read_timeout_us(rtc_i2c, DS3231_ADDRESS, buf, len, false, TIMEOUT_US);
}

/**
 * @brief convierte de decimal a bcd
 * @param val valor a convertir a bcd
 */
static uint8_t dec_to_bcd(uint8_t val) {
    return (val / 10 << 4) | (val % 10);
}

/**
 * @brief convierte de bcd a decimal
 * @param val valor a convertir a decimal
 */
static uint8_t bcd_to_dec(uint8_t val) {
    return (val & 0x0F) + ((val & 0xF0) >> 4) * 10;
}

/**
 * @brief lee valor de time de un registro
 * @param reg registro a leer
 * @return valor del registro en formato decimal
 */
uint8_t rtc_read_item(uint8_t reg) {
    uint8_t buf;
    i2c_write_timeout_us(rtc_i2c, DS3231_ADDRESS, &reg, 1, false, TIMEOUT_US);
    int8_t status = i2c_read_timeout_us(rtc_i2c, DS3231_ADDRESS, &buf, 1, false, TIMEOUT_US);
    if (status < 1) {
        return 99;
    }
    if (reg == RTC_HOURS) {
        // Si el bit 6 está activo, es formato 12h
        if (buf & (1 << 6)) 
            return bcd_to_dec(buf & 0x1F) + (buf & (1 << 5) ? 12 : 0); // Si el bit 5 está activo, es PM
        else
            return bcd_to_dec(buf & 0x3F);
    }
    return bcd_to_dec(buf & 0x7F);
}

/**
 * @brief inicializa el rtc
 * @param i2c i2c a utilizar
 */
void rtc_init_rtos(void *i2c_param) {
    i2c_inst_t *i2c = (i2c_inst_t *) i2c_param;
    rtc_i2c = i2c;
    uint8_t year = rtc_read_item(RTC_YEAR);
    if (year == 99) {
        printf("Error reading from RTC at address 0x%02X\n", DS3231_ADDRESS);
        return;
    }

    if (year == 0) {
        rtc_set_time(default_time);
    }
}

void rtc_set_i2c(i2c_inst_t *i2c) {
    rtc_i2c = i2c;
}

/**
 * @brief setea una hora
 * @param time fecha a guardar
 */
void rtc_set_time(time_t time) {
    if (time.year < YEAR_BASE || time.year > YEAR_BASE + 99) {
        printf("Year out of range. Must be between %d and %d.\n", YEAR_BASE, YEAR_BASE + 99);
        return;
    }
    if (time.month < 1 || time.month > 12) {
        printf("Month out of range. Must be between 1 and 12.\n");
        return;
    }
    if (time.day < 1 || time.day > 31) {
        printf("Day out of range. Must be between 1 and 31.\n");
        return;
    }
    if (time.hour > 23) {
        printf("Hour out of range. Must be between 0 and 23.\n");
        return;
    }
    if (time.minute > 59) {
        printf("Minute out of range. Must be between 0 and 59.\n");
        return;
    }
    if (time.second > 59) {
        printf("Second out of range. Must be between 0 and 59.\n");
        return;
    }
    uint8_t data[] = {
        RTC_SECONDS,
        dec_to_bcd(time.second),
        dec_to_bcd(time.minute),
        dec_to_bcd(time.hour),
        dec_to_bcd(time.day),
        dec_to_bcd(time.date),
        dec_to_bcd(time.month),
        dec_to_bcd((uint8_t) (time.year - YEAR_BASE))
    };
    uint8_t status = i2c_write_timeout_us(rtc_i2c, DS3231_ADDRESS, data, 8, false, TIMEOUT_US);
    if (status != 8) {
        printf("Error setting time on RTC.\n");
    }
}

/**
 * @brief lee la hora del rtc
 * @return fecha
 */
time_t rtc_get_time(void) {
    time_t fecha;
    fecha.year = rtc_read_item(RTC_YEAR) + YEAR_BASE;
    fecha.month = rtc_read_item(RTC_MONTH);
    fecha.date = rtc_read_item(RTC_DATE);
    fecha.day = rtc_read_item(RTC_DAY);
    fecha.hour = rtc_read_item(RTC_HOURS);
    fecha.minute = rtc_read_item(RTC_MINUTES);
    fecha.second = rtc_read_item(RTC_SECONDS);

    return fecha;
}

void rtc_get_time_rtos(void *param) {
    time_t *time = (time_t *) param;
    *time = rtc_get_time();
}

void rtc_set_time_rtos(void *param) {
    time_t *time = (time_t *) param;
    rtc_set_time(*time);
}
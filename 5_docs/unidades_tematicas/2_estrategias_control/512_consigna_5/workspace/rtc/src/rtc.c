#include "rtc.h"

// Puntero a I2C usado
static i2c_inst_t *rtc_i2c;

/**
 * @brief Manda un byte por I2C
 * @param val es el byte a mandar
*/
static void i2c_write_bytes(uint8_t reg, uint8_t val) {
    uint8_t datos[] = {reg, val};
    i2c_write_blocking(rtc_i2c, DS1307_ADDRESS, datos, 2, false);
}

/**
 * @brief Lee multiples bytes por I2C
 * @param reg es el registro inicial
 * @param len cantidad de bytes a leer
 * @param buf buffer de datos
 */
static void i2c_multiple_read(uint8_t reg, uint8_t len, uint8_t *buf) {
    i2c_write_blocking(rtc_i2c, DS1307_ADDRESS, &reg, 1, false);
    i2c_read_blocking(rtc_i2c, DS1307_ADDRESS, buf, len, false);
}

/**
 * @brief convierte de decimal a bcd
 * @param val valor a convertir a bcd
 */
static uint8_t dec2bcd(uint8_t val) {
    uint8_t i, j, k;
    i = val / 10;
    j = val % 10;
    k = j + (i << 4);
    return k;
}

/**
 * @brief convierte de bcd a decimal
 * @param val valor a convertir a decimal
 */
static uint8_t bcd2dec(uint8_t val) {
    uint8_t temp;
    temp = val & 0x0F;
    val = (val >> 4) & 0x0F;
    val = val * 10;
    temp = temp + val;
    return temp;
}

/**
 * @brief inicializa el rtc
 * @param i2c i2c a utilizar
 */
void rtc_init(i2c_inst_t *i2c) {
    rtc_i2c = i2c;

    // Leer el registro de segundos para preservar los bits 0-6
    uint8_t seg;
    i2c_multiple_read(RTC_SECONDS, 1, &seg);

    // Limpiar el bit 7 (CH - Clock Halt)
    seg &= ~(1 << 7);
    i2c_write_bytes(RTC_SECONDS, seg);

    // Registrar el control en 0x00 por las dudas
    i2c_write_bytes(RTC_CONTROL, 0x00);
}

/**
 * @brief setea una hora
 * @param time fecha a guardar
 */
void rtc_set_time(time_t time) {
    uint8_t year = time.year-YEAR_BASE;
    uint8_t data[] = {
        RTC_SECONDS,
        dec2bcd(time.second),
        dec2bcd(time.minute),
        dec2bcd(time.hour),
        dec2bcd(time.day),
        dec2bcd(time.date),
        dec2bcd(time.month),
        dec2bcd(year)
    };
    i2c_write_blocking(rtc_i2c, DS1307_ADDRESS, data, 8, false);
}

/**
 * @brief lee la hora del rtc
 * @return fecha
 */
time_t rtc_get_time(void) {
    time_t fecha;
    uint8_t buf[] = {0, 0, 0, 0, 0, 0, 0};
    i2c_multiple_read(RTC_SECONDS, 7, buf);
    fecha.year = bcd2dec(buf[6]) + YEAR_BASE;
    fecha.month = bcd2dec(buf[5] & 0x1F);
    fecha.date = bcd2dec(buf[4] & 0x3F);
    fecha.day = bcd2dec(buf[3] & 0x07);
    fecha.hour = bcd2dec(buf[2] & 0x3F);
    fecha.minute = bcd2dec(buf[1] & 0x7F);
    fecha.second = bcd2dec(buf[0] & 0x7F);
    return fecha;
}
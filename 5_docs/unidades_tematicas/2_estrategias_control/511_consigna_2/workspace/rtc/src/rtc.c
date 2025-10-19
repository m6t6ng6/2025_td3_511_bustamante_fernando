//Incluye funciones para RTC y EEPROM
#include "rtc.h"


static uint8_t dec_to_bcd(uint8_t val) {
    return ((val / 10) << 4) | (val % 10);
}

static uint8_t bcd_to_dec(uint8_t val) {
    return ((val >> 4) * 10) + (val & 0x0F);
}

void rtc_init(i2c_inst_t *i2c) {
}

void rtc_set_time(i2c_inst_t *i2c, const time_t *time) {
    uint8_t buffer[8];
    buffer[0] = 0x00;
    buffer[1] = dec_to_bcd(time->second);
    buffer[2] = dec_to_bcd(time->minute);
    buffer[3] = dec_to_bcd(time->hour);
    buffer[4] = dec_to_bcd(time->date);
    buffer[5] = dec_to_bcd(time->day);
    buffer[6] = dec_to_bcd(time->month);
    buffer[7] = dec_to_bcd(time->year);
    i2c_write_blocking(i2c, RTC_ADDR, buffer, 8, false);
}

void rtc_get_time(i2c_inst_t *i2c, time_t *time) {
    uint8_t reg = 0x00;
    uint8_t buffer[7];
    i2c_write_blocking(i2c, RTC_ADDR, &reg, 1, true);
    i2c_read_blocking(i2c, RTC_ADDR, buffer, 7, false);
    time->second       = bcd_to_dec(buffer[0] & 0x7F);
    time->minute       = bcd_to_dec(buffer[1]);
    time->hour         = bcd_to_dec(buffer[2] & 0x3F);
    time->date         = bcd_to_dec(buffer[3]);
    time->day          = bcd_to_dec(buffer[4]);
    time->month        = bcd_to_dec(buffer[5]);
    time->year         = bcd_to_dec(buffer[6]);
}

bool guardar_configuracion(const configuracion_t* conf) {
    uint8_t idx;

    // Leer índice actual
    eeprom_read(EEPROM_PTR_CONFIG, &idx, 1);

    // Calcular dirección de escritura
    uint16_t addr = EEPROM_ADDR_CONFIGS + (idx * sizeof(configuracion_t));

    // Escribir configuración
    eeprom_write(addr, (const uint8_t*)conf, sizeof(configuracion_t));

    // Incrementar índice circular
    idx = (idx + 1) % EEPROM_MAX_CONFIGS;

    // Guardar nuevo índice
    eeprom_write(EEPROM_PTR_CONFIG, &idx, 1);

    return true;
}

bool leer_ultima_configuracion(configuracion_t* conf) {
    uint8_t idx;

    // Leer índice actual
    eeprom_read(EEPROM_PTR_CONFIG, &idx, 1);

    // Si es la primera vez, aún no se escribió nada
    if (idx == 0 && EEPROM_MAX_CONFIGS == 0)
        return false;

    if (idx == 0) idx = EEPROM_MAX_CONFIGS;
    idx--;

    uint16_t addr = EEPROM_ADDR_CONFIGS + (idx * sizeof(configuracion_t));

    eeprom_read(addr, (uint8_t*)conf, sizeof(configuracion_t));
    return true;
}

bool guardar_resultado(const resultado_t* res) {
    uint8_t idx;

    eeprom_read(EEPROM_PTR_ADDR, &idx, 1);

    uint16_t addr = EEPROM_ADDR_RESULT + (idx * sizeof(resultado_t));
    eeprom_write(addr, (const uint8_t*)res, sizeof(resultado_t));

    idx = (idx + 1) % EEPROM_MAX_RESULTS;
    eeprom_write(EEPROM_PTR_ADDR, &idx, 1);

    return true;
}

bool leer_ultimo_resultado(resultado_t* res) {
    uint8_t idx;

    // Leer índice actual
    eeprom_read(EEPROM_PTR_ADDR, &idx, 1);

    // Si es la primera vez, el valor leído puede ser 0xFF (valor por defecto en EEPROM no escrita)
    if (idx == 0xFF || idx >= EEPROM_MAX_RESULTS)
        return false;

    if (idx == 0) idx = EEPROM_MAX_RESULTS;
    idx--;

    uint16_t addr = EEPROM_ADDR_RESULT + (idx * sizeof(resultado_t));
    eeprom_read(addr, (uint8_t*)res, sizeof(resultado_t));

    return true;
}

// Lee un byte desde la RAM del DS3231
bool eeprom_read_bytes(i2c_inst_t *i2c, uint8_t address, uint8_t *data) {
    if (address < 0x08 || address > 0x3F) return false;

    i2c_write_blocking(i2c, RTC_ADDR, &address, 1, true);
    return (i2c_read_blocking(i2c, RTC_ADDR, data, 1, false) == 1);
}

void eeprom_write(uint16_t addr, const uint8_t *data, size_t len) {
    for (size_t i = 0; i < len; i++) {
        uint8_t buf[3];
        buf[0] = (addr >> 8) & 0xFF; // Dirección alta
        buf[1] = addr & 0xFF;        // Dirección baja
        buf[2] = data[i];            // Byte a escribir

        i2c_write_blocking(I2C_PORT, EEPROM_ADDR, buf, 3, false);
        sleep_ms(5);
        addr++;
    }
}

void eeprom_read(uint16_t addr, uint8_t *data, size_t len) {
    uint8_t addr_buf[2] = {
        (addr >> 8) & 0xFF,
        addr & 0xFF
    };
    i2c_write_blocking(I2C_PORT, EEPROM_ADDR, addr_buf, 2, true);
    i2c_read_blocking(I2C_PORT, EEPROM_ADDR, data, len, false);
}
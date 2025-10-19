//Incluye funciones para RTC y EEPROM
#ifndef _RTC_H_
#define _RTC_H_

#include "hardware/i2c.h"
#include <stdint.h>
#include <stdbool.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"

#define RTC_ADDR 0x68
#define EEPROM_ADDR 0x57
#define I2C_PORT      i2c0      // I2C usado

// EEPROM: direcciones base y parámetros
#define EEPROM_ADDR_CONFIGS     0x0200  // inicio del área para configuraciones
#define EEPROM_PTR_CONFIG       0x00FD  // puntero a última configuración guardada
#define EEPROM_MAX_CONFIGS      5       // cantidad máxima de configuraciones guardadas

#define EEPROM_ADDR_RESULT      0x0300  // inicio del área para resultados
#define EEPROM_PTR_ADDR         0x00FE  // puntero a último resultado guardado
#define EEPROM_MAX_RESULTS      10      // cantidad máxima de resultados guardados

typedef struct __attribute__((packed)) {
    uint16_t year;
    uint8_t month;
    uint8_t date;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
} time_t;

typedef struct __attribute__((packed)) {
    float setpoint;
    float pendiente;
    bool tipo_entrada; // 0 = escalón, 1 = rampa
    time_t fecha;
} configuracion_t;

typedef struct {
    float angulo;
    float setpoint;
    float error;
    float salida_control;
    bool tipo_entrada; // 0 = escalón, 1 = rampa
    bool flag_led;
    time_t fecha;
} resultado_t;

void rtc_init(i2c_inst_t *i2c);
void rtc_set_time(i2c_inst_t *i2c, const time_t *time);
void rtc_get_time(i2c_inst_t *i2c, time_t *time);

bool eeprom_write_bytes(i2c_inst_t *i2c, uint8_t address, uint8_t data);
bool eeprom_read_bytes(i2c_inst_t *i2c, uint8_t address, uint8_t *data);

// Funciones para escribir y leer en la EEPROM
void eeprom_write(uint16_t addr, const uint8_t *data, size_t len);
void eeprom_read(uint16_t addr, uint8_t *data, size_t len);

// Funciones EEPROM (para configuración y resultados)
bool guardar_configuracion(const configuracion_t* conf);
bool leer_ultima_configuracion(configuracion_t* conf);
bool guardar_resultado(const resultado_t* res);
bool leer_ultimo_resultado(resultado_t* res);

#endif
#include "bmp280.h"

// Puntero a I2C usado
static i2c_inst_t *i2c_bmp;

/**
 * @brief Inicialización del BMP280
 * @param i2c puntero a instancia de I2C
 */
void bmp280_init(i2c_inst_t *i2c) {
  // Guardo el I2C elegido
  i2c_bmp = i2c;
  uint8_t buf[2];

  // 500ms de tiempo de sampling, x16 filter
  const uint8_t reg_config_val = ((0x04 << 5) | (0x05 << 2)) & 0xFC;

  // Registro y luego el valor a escribir
  buf[0] = REG_CONFIG;
  buf[1] = reg_config_val;
  i2c_write_blocking(i2c_bmp, ADDR, buf, 2, false);

  // osrs_t x1, osrs_p x4, normal mode operation
  const uint8_t reg_ctrl_meas_val = (0x01 << 5) | (0x03 << 2) | (0x03);
  buf[0] = REG_CTRL_MEAS;
  buf[1] = reg_ctrl_meas_val;
  i2c_write_blocking(i2c_bmp, ADDR, buf, 2, false);
}

/**
 * @brief Software reset
 */
void bmp280_reset() {
    // Resetear dispositivo
    uint8_t buf[2] = { REG_RESET, 0xB6 };
    i2c_write_blocking(i2c_bmp, ADDR, buf, 2, false);
}

/**
 * @brief Obtiene los valores de calibración del BMP280
 * @param params puntero a estructura de parámetros de calibración
 */
void bmp280_get_calib_params(struct bmp280_calib_param* params) {
    // Los parámetros de calibración son propios del proceso de fabricación del sensor
    // Hay 3 parámetros de temperatura y 9 de presión, todos con un registro LSB y MSB
    // Hay que leer 24 registros
    uint8_t buf[NUM_CALIB_PARAMS] = { 0 };
    uint8_t reg = REG_DIG_T1_LSB;
    // true para mantener control del bus en el master
    i2c_write_blocking(i2c_bmp, ADDR, &reg, 1, true);
    // Leer los 24 registros que se autoincrementan
    i2c_read_blocking(i2c_bmp, ADDR, buf, NUM_CALIB_PARAMS, false);

    // Guardar en la estructura
    params->dig_t1 = (uint16_t)(buf[1] << 8) | buf[0];
    params->dig_t2 = (int16_t)(buf[3] << 8) | buf[2];
    params->dig_t3 = (int16_t)(buf[5] << 8) | buf[4];

    params->dig_p1 = (uint16_t)(buf[7] << 8) | buf[6];
    params->dig_p2 = (int16_t)(buf[9] << 8) | buf[8];
    params->dig_p3 = (int16_t)(buf[11] << 8) | buf[10];
    params->dig_p4 = (int16_t)(buf[13] << 8) | buf[12];
    params->dig_p5 = (int16_t)(buf[15] << 8) | buf[14];
    params->dig_p6 = (int16_t)(buf[17] << 8) | buf[16];
    params->dig_p7 = (int16_t)(buf[19] << 8) | buf[18];
    params->dig_p8 = (int16_t)(buf[21] << 8) | buf[20];
    params->dig_p9 = (int16_t)(buf[23] << 8) | buf[22];
}

/**
 * @brief Lee los valores de presión y temperatura sin compensar
 * @param raw_temp puntero donde guardar la temperatura
 * @param raw_pressure puntero donde guardar la presión
 */
void bmp280_read_raw(int32_t* raw_temp, int32_t* raw_pressure) {
    // Los registros dl BMP280 se autoincrementan
    // Hay 3 de temperatura y 3 de presión, se arranca en 0xf7 y se leen 6 bytes hasta 0xfc
    uint8_t buf[6];
    uint8_t reg = REG_PRESSURE_MSB;
    // true para mantener el control del bus desde el master
    i2c_write_blocking(i2c_bmp, ADDR, &reg, 1, true);
    // false porque se terminó la transacción
    i2c_read_blocking(i2c_bmp, ADDR, buf, 6, false);

    // Se arman los 20 bits de datos en variable con signo de 32 bits
    *raw_pressure = (buf[0] << 12) | (buf[1] << 4) | (buf[2] >> 4);
    *raw_temp = (buf[3] << 12) | (buf[4] << 4) | (buf[5] >> 4);
}

/**
 * @brief Obtiene la resolución fina de temperatura según la sección 8.2 del datasheet
 * @param raw_temp valor de temperatura sin compensar
 * @param params puntero a parámetros de calibración
 * @return resolución fina de temperatura
 */
static int32_t bmp280_convert(int32_t raw_temp, struct bmp280_calib_param* params) {

    int32_t var1, var2;
    var1 = ((((raw_temp >> 3) - ((int32_t)params->dig_t1 << 1))) * ((int32_t)params->dig_t2)) >> 11;
    var2 = (((((raw_temp >> 4) - ((int32_t)params->dig_t1)) * ((raw_temp >> 4) - ((int32_t)params->dig_t1))) >> 12) * ((int32_t)params->dig_t3)) >> 14;
    return var1 + var2;
}

/**
 * @brief Obtiene el valor compensado de temperatura según la sección 8.2 del datasheet
 * @param raw_temp valor de tempratura sin compensar
 * @param params puntero a parámetros de calibración
 * @return temperatura compensada en grados Celsius
 */
float bmp280_convert_temp(int32_t raw_temp, struct bmp280_calib_param* params) {
    // Compensa el valor de temperatura de los registros
    int32_t t_fine = bmp280_convert(raw_temp, params);
    return ((t_fine * 5 + 128) >> 8) / 100.0;
}

/**
 * @brief Obtiene el valor compensado de presión según la sección 8.2 del datasheet
 * @param raw_pressure valor de presión sin compensar
 * @param raw_temp valor de tempratura sin compensar
 * @param params puntero a parámetros de calibración
 * @return valor compensado de presión en Pascales
 */
int32_t bmp280_convert_pressure(int32_t raw_pressure, int32_t raw_temp, struct bmp280_calib_param* params) {
    // Compensa el valor de temperatura de los registros
    int32_t t_fine = bmp280_convert(raw_temp, params);
    // Compensa la presión
    int32_t var1, var2;
    uint32_t converted = 0.0;
    var1 = (((int32_t)t_fine) >> 1) - (int32_t)64000;
    var2 = (((var1 >> 2) * (var1 >> 2)) >> 11) * ((int32_t)params->dig_p6);
    var2 += ((var1 * ((int32_t)params->dig_p5)) << 1);
    var2 = (var2 >> 2) + (((int32_t)params->dig_p4) << 16);
    var1 = (((params->dig_p3 * (((var1 >> 2) * (var1 >> 2)) >> 13)) >> 3) + ((((int32_t)params->dig_p2) * var1) >> 1)) >> 18;
    var1 = ((((32768 + var1)) * ((int32_t)params->dig_p1)) >> 15);
    if (var1 == 0) {
        return 0;  // avoid exception caused by division by zero
    }
    converted = (((uint32_t)(((int32_t)1048576) - raw_pressure) - (var2 >> 12))) * 3125;
    if (converted < 0x80000000) {
        converted = (converted << 1) / ((uint32_t)var1);
    } else {
        converted = (converted / (uint32_t)var1) * 2;
    }
    var1 = (((int32_t)params->dig_p9) * ((int32_t)(((converted >> 3) * (converted >> 3)) >> 13))) >> 12;
    var2 = (((int32_t)(converted >> 2)) * ((int32_t)params->dig_p8)) >> 13;
    converted = (uint32_t)((int32_t)converted + ((var1 + var2 + params->dig_p7) >> 4));
    return converted;
}
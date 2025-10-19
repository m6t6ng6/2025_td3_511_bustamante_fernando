#include "bh1750.h"


/**
 * @brief Inicializa el bh1750
 * 
 */
void bh1750_init(void) {
    // Prendo el bh1750 y le fijo la resolucion
    uint8_t config[] = {0x10};  // 1 lux de resolucion, con 120ms de muestreo
    i2c_write_blocking(I2C_PORT, BH1750_ADDR, config, 1, false);
}

/**
 * @brief Obtiene el valor en lux
 * 
 * @return uint16_t lux
 */
uint16_t bh1750_read_lux(void) {
    uint8_t buf[2];
    i2c_read_blocking(I2C_PORT, BH1750_ADDR, buf, 2, false);
    uint16_t lux = (buf[0] << 8) | buf[1];
    // Divido por 1.2 para convertir en Lux
    return lux;
}
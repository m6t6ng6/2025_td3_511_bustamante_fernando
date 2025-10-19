#include "hardware/i2c.h"


#define BH1750_ADDR 0x23

/**
 * @brief Inicializa el bh1750
 * 
 */
void bh1750_init(void);

/**
 * @brief Obtiene el valor en lux
 * 
 * @return uint16_t lux
 */
uint16_t bh1750_read_lux(void);
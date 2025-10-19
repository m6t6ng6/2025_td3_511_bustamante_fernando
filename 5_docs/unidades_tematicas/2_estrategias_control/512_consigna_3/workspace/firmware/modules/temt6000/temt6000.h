#define DEFAULT_CTE_LUX 2.0f
#define RESISTOR        10e3f
#define ADC_RESOLUTION  4096
#define V_REF           3.3f

#include "pico/stdlib.h"
#include "hardware/adc.h"

/**
 * @brief Inicializa el adc en el pin del temt6000
 * 
 * @param pin 
 * @param chann 
 */
void temt6000_init(uint8_t pin, uint8_t chann);

/**
 * @brief Calcula el voltaje a partir de la medicion del adc
 * 
 * @param adc 
 * @return float voltaje
 */
float adc_get_voltage(uint16_t adc_raw);

/**
 * @brief Obtiene la corriente a partir del voltage
 * 
 * @param voltage 
 * @return float corriente sobre la resistencia de sensado
 */
float temt6000_get_current(float voltage);

/**
 * @brief Obtiene los lux
 * @param voltage
 * @return float lux
 */

float temt6000_get_lux(void);


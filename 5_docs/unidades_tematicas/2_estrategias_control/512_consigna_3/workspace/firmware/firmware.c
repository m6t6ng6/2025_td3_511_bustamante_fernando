/**
 * @file firmware.c
 * @author Franco Lopez & Matias Pietras (francoalelopez@gmail.com mati_pietras@yahoo.com.ar)

 * @brief 
 * @version 0.1
 * @date 2025-06-30
 * 
 * @copyright Copyright (c) 2025
 * 
 */
#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/adc.h"

#include "modules/temt6000/temt6000.h"
#include "modules/bh1750/bh1750.h"

#define I2C_FREQ 400*1000

#define PIN_TEMT6000 28
#define CHANN_TEMT6000 2


void i2c_config(void){
    i2c_init(I2C_PORT, I2C_FREQ);
    
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);


}

void adc_config(void){
    adc_gpio_init(PIN_TEMT6000);
    adc_select_input(CHANN_TEMT6000);

}

int main() {
    stdio_init_all();


    adc_init();
    i2c_config();
    bh1750_init();
    
    while (1) {
        printf("Lux TEMT6000:%f\n", temt6000_get_lux());
        uint16_t lux = bh1750_read_lux();
        printf("Luz BH1750: %u lux\n", lux);
        sleep_ms(500);
    }
}


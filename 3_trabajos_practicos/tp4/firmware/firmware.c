#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include <stdio.h>
#include "bmp280.h"
#include "lcd.h"

#define I2C_PORT    i2c0    // canal i2c utilizado
#define SDA_PIN     8       // gpio 8 para SDA
#define SCL_PIN     9       // gpio 9 para SCL
#define ADDRESS     0x27    // lcd i2c address

SemaphoreHandle_t i2c_mutex;

QueueHandle_t cola_datos;

typedef struct {
    int32_t raw_temp;
    int32_t raw_pressure;
    float temp_c;
    int32_t pressure_pa;
} sensor_data_t;

// Función para inicializar el puerto I2C con pull-ups internos
void init_i2c(void) {
    // Inicializar el puerto I2C con frecuencia de 100 kHz
    i2c_init(I2C_PORT, 100000);

    // Configurar pines SDA y SCL para función I2C
    gpio_set_function(SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(SCL_PIN, GPIO_FUNC_I2C);

    // Activar resistencias pull-up internas
    gpio_pull_up(SDA_PIN);
    gpio_pull_up(SCL_PIN);
}

void task_read_sensor(void *pvParameters) {

    // Obtener parámetros de calibración del sensor
    struct bmp280_calib_param params;
    bmp280_get_calib_params(&params);

    int32_t raw_temp, raw_pressure;
    float temp_c;
    int32_t pressure_pa;
    sensor_data_t data;

    int16_t contador = 1;

    while(1){

        xSemaphoreTake(i2c_mutex, portMAX_DELAY);
        bmp280_read_raw(&raw_temp, &raw_pressure);
        data.raw_temp = raw_temp;
        data.raw_pressure = raw_pressure;
        data.temp_c = bmp280_convert_temp(raw_temp, &params);
        data.pressure_pa = bmp280_convert_pressure(raw_pressure, raw_temp, &params);
        xQueueSend(cola_datos, &data, portMAX_DELAY);
        printf("%lu - raw_temp: %lu | raw_pressure: %lu | temp_c: %.2f C | pressure_pa: %lu Pa", contador, data.raw_temp, data.raw_pressure, data.temp_c, data.pressure_pa);
        contador++;
        xSemaphoreGive(i2c_mutex);
        vTaskDelay(pdMS_TO_TICKS(1000));

    }

}

void task_write_lcd(void *pvParameters) {

    sensor_data_t data;

    while(1){

        xSemaphoreTake(i2c_mutex, portMAX_DELAY);
        if (xQueueReceive(cola_datos, &data, portMAX_DELAY) == pdPASS) {
            // Limpio el LCD
            lcd_clear();
            // Muevo el cursor a la primera fila, columna cero
            lcd_set_cursor(0, 0);
            // Escribo 
            char buffer[20];
            sprintf(buffer, "TEMP: %.2f C", data.temp_c);
            lcd_string(buffer);
            // Muevo el cursor a la segunda fila, columna cero
            lcd_set_cursor(1, 0);
            // Escribo
            char buffer1[20];
            sprintf(buffer1, "PRESION: %lu Pa", data.pressure_pa);
            lcd_string(buffer1);
        }
        printf(" >>> LCD imprime datos\n");
        xSemaphoreGive(i2c_mutex);
        vTaskDelay(pdMS_TO_TICKS(1000));

    }

}

int main() {
    stdio_init_all();   // inicializacion de 

    init_i2c(); // inicializacion del canal i2c

    bmp280_init(I2C_PORT);  // inicializacion del sensor

    lcd_init(I2C_PORT, ADDRESS);    // inicializacion del lcd

    i2c_mutex = xSemaphoreCreateMutex();    // se crea el semaforo mutex
    if (i2c_mutex == NULL) {
        printf("Error al crear mutex\n");
    while (1);
    }

    cola_datos = xQueueCreate(1, sizeof(sensor_data_t));    // cola que posee una unica posicion de la estructura sensor_data_t

    xTaskCreate(task_read_sensor, "read_sensor", 2048, NULL, 1, NULL);  // crea tarea para leer el sensor
    xTaskCreate(task_write_lcd, "write_lcd", 2048, NULL, 1, NULL);  // crea tarea para escribir en el lcd

    vTaskStartScheduler();  // inicia el scheduler de freertos

    while (1) {};   // nunca entra en este loop mientras el scheduler este activo
}

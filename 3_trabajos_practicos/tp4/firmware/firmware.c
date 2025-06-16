#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include <stdio.h>
#include "bmp280.h"
#include "lcd.h"
#include "stdbool.h"
#include "helper.h"

#define I2C_PORT    i2c0    // canal i2c utilizado
#define SDA_PIN     8       // gpio 8 para SDA
#define SCL_PIN     9       // gpio 9 para SCL
#define ADDRESS     0x27    // lcd i2c address
#define SETPOINT    25      // set point
#define PIN_SWITCH  14      // pin boton
#define PIN_PWM     10      // pin pwm

SemaphoreHandle_t i2c_mutex;

QueueHandle_t cola_datos;
QueueHandle_t cola_paginas;

bool pagina = false;  // false = sensor, true = set point & error

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

void task_monitor_gpio(void *pvParameters) {
    while (1) {
        printf("Estado GPIO14: %d\n", gpio_get(PIN_SWITCH));
        vTaskDelay(pdMS_TO_TICKS(100));
        //if (gpio_get(PIN_SWITCH)) {
        //    printf("GPIO14 en HIGH, forzando reinicio de configuración\n");
        //    gpio_disable_pulls(PIN_SWITCH);
        //    gpio_set_function(PIN_SWITCH, GPIO_FUNC_NULL);
        //    sleep_ms(10);
        //    gpio_init(PIN_SWITCH);
        //    gpio_set_dir(PIN_SWITCH, GPIO_IN);
        //    gpio_pull_down(PIN_SWITCH);
        //}
    }
}

void task_debounce_boton(void *pvParameters) {
    bool last_state = false;
    bool stable_state = false;
    TickType_t last_debounce_time = 0;
    const TickType_t debounce_delay = pdMS_TO_TICKS(50);  // 50 ms debounce

    while (1) {
        bool current_state = gpio_get(PIN_SWITCH);

        if (current_state != last_state) {
            last_debounce_time = xTaskGetTickCount();
        }

        if ((xTaskGetTickCount() - last_debounce_time) > debounce_delay) {
            if (current_state != stable_state) {
                stable_state = current_state;

                if (stable_state == true) {  // flanco ascendente limpio
                    xQueuePeek(cola_paginas, &pagina, 0);
                    printf("\nBOTON DETECTADO - estado anterior: %s\n", pagina ? "true" : "false");
                    pagina = !pagina;
                    printf("BOTON DETECTADO - estado nuevo: %s\n", pagina ? "true" : "false");
                    xQueueOverwrite(cola_paginas, &pagina);
                }
            }
        }

        last_state = current_state;
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void boton_callback(uint gpio, uint32_t events) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    if (gpio == PIN_SWITCH && (events & GPIO_IRQ_EDGE_RISE)) {
        xQueuePeekFromISR(cola_paginas, &pagina);
        printf("\nWARNING: estado actual: %s\n", pagina ? "true" : "false");
        pagina = !pagina;
        printf("WARNING: estado nuevo: %s\n", pagina ? "true" : "false" );
        xQueueOverwriteFromISR(cola_paginas, &pagina, &xHigherPriorityTaskWoken);
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
}

void configuracion_gpio_boton(void) {
    // Configuración del botón en GPIO 14
    gpio_init(PIN_SWITCH);
    gpio_set_dir(PIN_SWITCH, GPIO_IN);
    gpio_pull_down(PIN_SWITCH);   // configura pull down 
    //gpio_set_irq_enabled_with_callback(PIN_SWITCH, GPIO_IRQ_EDGE_RISE, true, &boton_callback);   // cuando detecta evento, \
                                                                                            evento: GPIO_IRQ_EDGE_RISE (flanco ascendente), \
                                                                                            TRUE: habilita la interrupcion para este GPIO, \
                                                                                            va a la dirección de memoria en la que la función "boton_callback" se encuentra
}

void configuraion_gpio_pwm(void) {
    gpio_set_function(PIN_PWM, GPIO_FUNC_PWM);
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
            
        }
        if (xQueuePeek(cola_paginas, &pagina, portMAX_DELAY) == pdPASS) {
        
        }    

        if (pagina == false) {
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
        } else {
            lcd_clear();
            lcd_set_cursor(0,0);
            char buffer[20];
            sprintf(buffer, "Set Point %lu C", SETPOINT);
            lcd_string(buffer);
            // Muevo el cursor a la segunda fila, columna cero
            lcd_set_cursor(1, 0);
            // Escribo
            sprintf(buffer, "ERROR: %.2f %%", (((data.temp_c) - (float) SETPOINT) / data.temp_c) * 100);
            lcd_string(buffer);
           
            // porcion de codigo que controla el duty cycle del GPIO del LED
            uint slice_num = pwm_gpio_to_slice_num(PIN_PWM);   // determinacion a que slice pertenece el PIN_PWM, segun tabla: GPIO 10 es slice 5, canal A
            pwm_set_wrap(slice_num, 1000);   // define el tope del wrap, duty cycle 0 -> wrap: 0, duty cycle 100 -> wrap: 1000
            pwm_set_chan_level(slice_num, PWM_CHAN_A, (((data.temp_c) - (float) SETPOINT) / data.temp_c) * 1000 );  // varia el valor del duty cycle en funcion del error
            pwm_set_enabled(slice_num, true);  // habilita el pwm con el slice indicado
            
        }

        printf(" >>> LCD imprime datos\n");
        xSemaphoreGive(i2c_mutex);
        vTaskDelay(pdMS_TO_TICKS(1000));

    }

}

int main() {

    stdio_init_all();

    configuracion_gpio_boton();  // configuración de puerto GPIO 14 para introducir funcionalidad en el boton NA
    
    configuraion_gpio_pwm();

    init_i2c(); // inicializacion del canal i2c

    bmp280_init(I2C_PORT);  // inicializacion del sensor

    lcd_init(I2C_PORT, ADDRESS);    // inicializacion del lcd

    i2c_mutex = xSemaphoreCreateMutex();    // se crea el semaforo mutex
    if (i2c_mutex == NULL) {
        printf("Error al crear mutex\n");
        while (1);
    }

    cola_datos = xQueueCreate(1, sizeof(sensor_data_t));    // cola que posee una unica posicion de la estructura sensor_data_t
    cola_paginas = xQueueCreate(1, sizeof(pagina));   // cola que posee una unica posicion para memorizar el cambio de paginas

    xQueueOverwrite(cola_paginas, &pagina);

    xTaskCreate(task_read_sensor, "read_sensor", 2048, NULL, 1, NULL);  // crea tarea para leer el sensor
    xTaskCreate(task_write_lcd, "write_lcd", 2048, NULL, 1, NULL);  // crea tarea para escribir en el lcd
    xTaskCreate(task_debounce_boton, "debounce_boton", 1024, NULL, 1, NULL);
    xTaskCreate(task_monitor_gpio, "monitor_gpio", 512, NULL, 1, NULL);

    vTaskStartScheduler();  // inicia el scheduler de freertos

    while (1) {};   // nunca entra en este loop mientras el scheduler este activo
}

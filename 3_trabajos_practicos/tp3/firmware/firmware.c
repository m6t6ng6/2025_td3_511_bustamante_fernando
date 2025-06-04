#include "pico/stdlib.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "stdio.h"

#include "lcd.h"
#include "helper.h"

// Configuraciones
#define GPIO_INPUT             1
#define PERIODO_MEDICION_MS    1000

// Eleccion de I2C a usar
#define I2C         i2c0
// Eleccion de GPIO para SDA
#define SDA_GPIO    8
// Eleccion de GPIO para SCL
#define SCL_GPIO    9
// Direccion de 7 bits del adaptador del LCD
#define ADDR        0x27

// Definición de colas
QueueHandle_t cola_flancos;
QueueHandle_t cola_frecuencia;

// ISR: Detecta flanco ascendente y manda un 1 a la cola
void gpio_callback(uint gpio, uint32_t events) {
    if (gpio == GPIO_INPUT && (events & GPIO_IRQ_EDGE_RISE)) {   // ingresa al IF si el puerto es el correcto y si es por un flanco ascendente
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;   // toma control para que ninguna tarea de mayor prioridad se ejecute, \
                                                            extremadamente necesario para mantener predecible el flujo
        uint8_t flanco = 1;   // se crea variable flanco y se le coloca un 1
        xQueueSendFromISR(cola_flancos, &flanco, &xHigherPriorityTaskWoken);   // se carga el valor 1 en la cola_flancos desde el ISR y \
                                                                                    esta API pone la variable xHigherPriorityTaskWoken en True para que la tarea que est bloqueada esperando el dato de la cola y permita que tome el control 
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);   // es necesario para que salga del ISR automaticamente
    }
}

// Tarea que acumula flancos y calcula frecuencia cada segundo
void tarea_frecuencimetro(void *pvParameters) {
    uint32_t contador = 0;
    uint8_t dummy;

    TickType_t xLastWakeTime = xTaskGetTickCount();

    while (1) {
        // Contar flancos durante PERIODO_MEDICION_MS
        while (xTaskGetTickCount() < xLastWakeTime + pdMS_TO_TICKS(PERIODO_MEDICION_MS)) {
            if (xQueueReceive(cola_flancos, &dummy, pdMS_TO_TICKS(10)) == pdPASS) {
                contador++;
            }
        }

        // Enviar resultado a cola de impresión
        xQueueSend(cola_frecuencia, &contador, portMAX_DELAY);

        contador = 0;
        xLastWakeTime += pdMS_TO_TICKS(PERIODO_MEDICION_MS);
    }
}

// Tarea que imprime frecuencia recibida por consola USB
void tarea_lcd(void *pvParameters) {
    uint32_t frecuencia;

    while (1) {
        if (xQueueReceive(cola_frecuencia, &frecuencia, portMAX_DELAY) == pdPASS) {
            printf("Frecuencia medida: %lu Hz\n", frecuencia);
            // Limpio el LCD
            lcd_clear();
            // Escribo al comienzo
            lcd_string("Freq medicion:");
            // Muevo el cursor a la segunda fila, tercer columna
            lcd_set_cursor(3, 2);
            // Escribo
            char buffer[20];
            sprintf(buffer, "%lu Hz", frecuencia);
            lcd_string(buffer);
        }
    }
}

// Inicialización GPIO
void gpio_configurar() {
    gpio_init(GPIO_INPUT);   // habilito GPIO en el GPIO 1
    gpio_set_dir(GPIO_INPUT, GPIO_IN);  // transformo en entrada el puerto GPIO 1
    gpio_pull_down(GPIO_INPUT);   // por defecto toma valores 0 si no hay señal
}

// Función principal
int main() {
    stdio_init_all();
    gpio_configurar();   // invoca funcion para inicializar puerto de entrada

    pwm_user_init(0, 10000);   // inicio PWM en GPIO 0
    // Inicializo el I2C con un clock de 100 KHz
    i2c_init(I2C, 100000);
    // Habilito la funcion de I2C en los GPIOs
    gpio_set_function(SDA_GPIO, GPIO_FUNC_I2C);
    gpio_set_function(SCL_GPIO, GPIO_FUNC_I2C);
    // Habilito pull-ups
    gpio_pull_up(SDA_GPIO);
    gpio_pull_up(SCL_GPIO);
    // Inicializo LCD
    lcd_init(I2C, ADDR);

    // Crear colas
    cola_flancos = xQueueCreate(1024, sizeof(uint8_t));    // 1024 flancos posibles por segundo
    cola_frecuencia = xQueueCreate(4, sizeof(uint32_t));   // Buffer pequeño, 1 dato por segundo

    // Registrar interrupción
    gpio_set_irq_enabled_with_callback(GPIO_INPUT, GPIO_IRQ_EDGE_RISE, true, &gpio_callback);   // interfaz de entrada: GPIO_INPUT, \
                                                                                                    event_mask: (GPIO_IRQ_EDGE_RISE: IRQ cuando la señal transiciona desde 0 a 1), \
                                                                                                    enabled: (true: habilitado), \
                                                                                                    callback: (gpio_callback: funcion que se llama cuando se detecta el evento)

    // Crear tareas
    xTaskCreate(tarea_frecuencimetro, "Contador_Flancos", 1024, NULL, 2, NULL);
    xTaskCreate(tarea_lcd, "Impresion_LCD", 1024, NULL, 1, NULL);

    // Iniciar el scheduler
    vTaskStartScheduler();

    while (1);

}
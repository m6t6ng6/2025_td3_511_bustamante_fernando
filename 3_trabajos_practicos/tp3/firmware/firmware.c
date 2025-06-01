#include "pico/stdlib.h"
#include "stdio.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "queue.h"

#include "helper.h"

#define GPIO_ENTRADA        1
#define TIEMPO_MEDICION_MS  10000
#define PERIODO_MUESTREO_MS 1

typedef struct {
    float frecuencia;
    float tiempo_medicion;
    uint32_t raw;
} datos_medidos;

#pragma GCC optimize ("O0")    // habilito el debugger para que muestre las variables

SemaphoreHandle_t sem_flancos;   // tipo de dato semaforo
QueueHandle_t queue_frecuencia;   // tipo de dato cola

// Tarea que muestrea el GPIO y detecta flancos ascendentes
void task_muestreo_gpio(void *pvParameters) {
    bool estado_anterior = false;  // guardo el estado anterior del GPIO de entrada

    while(1) {
        bool estado_actual = gpio_get(GPIO_ENTRADA);   // guardo el estado actual del GPIO de entrada, FALSE: 0, TRUE: 1

        // Detectar flanco ascendente
        if (!estado_anterior && estado_actual) {    // entra al bloque solo si el estado anterior es distinto al estado actual
            if ( estado_anterior == false ) {
                xSemaphoreGive(sem_flancos);   // incremento en una unidad el contador interno del semaforo
            }
            if ( estado_anterior == true ) {
                ;  // si es un flanco descendente no hace nada 
            }
        }

        estado_anterior = estado_actual;     // carga el estado actual en el estado anterior
        vTaskDelay(pdMS_TO_TICKS(PERIODO_MUESTREO_MS));   // bloquea la tarea por el tiempo dado, en ete caso 1 segundo, \
                                                            se desbloquea cuando expira el tiempo y vuelve a tomar una muestra
    }
}

// Tarea que mide la frecuencia cada segundo y la envía a una cola
void task_frecuencia(void *pvParameters) {
    datos_medidos datos;

    while(1) {
        uint32_t contador = 0;   // crea contador y lo incializa en 0, TIPO DE DATO: entero sin signo de 32 bits
        TickType_t tiempo_inicio = xTaskGetTickCount();   // obtiene la cantidad de ticks desde que se inicio el scheduler, tiempo de inicio como referencia

        while ( xTaskGetTickCount() - tiempo_inicio < pdMS_TO_TICKS(TIEMPO_MEDICION_MS)) {   // compara el tiempo de inicio con el tiempo actual, si es menor al tiempo de medicion definido
            if (xSemaphoreTake(sem_flancos, pdMS_TO_TICKS(10)) == pdTRUE) {   // si el semaforo contador esta disponible para ser tomado, xSemaphore: sem_flancos, xTicksToWait: equivalente a 10 ms
                contador++;   // incrementa contador
                datos.tiempo_medicion = (float) (xTaskGetTickCount() - tiempo_inicio);
            }
        }

        datos.raw = contador;
        datos.frecuencia = (float)contador * 1000.00f * 100.00f / (float)TIEMPO_MEDICION_MS;    // calculo de frecuencia: \
                                                                                            numero de cuentas      \
                                                                                            -----------------        *   1000   [Hz]\
                                                                                            tiempo de medicion (ms)

        // Enviar a la cola
        xQueueSend(queue_frecuencia, &datos, 0);   // escribe el dato de la variable frecuencia en la cola queue_frecuencia, \
                                                            xTicksToWait: 0 (que sea inmediata la escritura, o sea que se bloquee 0 ms)
    }
}

// Tarea que imprime por consola la frecuencia recibida
void task_imprimir_usb(void *pvParameters) {
    datos_medidos datos;   // creo variable de frecuencia recibida

    uint16_t row = 0;

    while(1) {
        if (xQueueReceive(queue_frecuencia, &datos, portMAX_DELAY) == pdTRUE) {   // xQueue: recibe el item de la cola queue_frecuencia, \
                                                                                                    pvBuffer: lo guarda en la variable frecuencia_recibida \
                                                                                                    xTicksToWait: espera indefinidamente, o sea queda bloqueada indefinidamente hasta que la cola este disopnible
            printf("muestra: %u | frecuencia: %.2f Hz | tiempo: %.2f ms | raw: %u\n", row, datos.frecuencia, datos.tiempo_medicion, datos.raw);    // imprime el dato por consola usb
            row++;
        }
    }
}

// Inicialización GPIO
void gpio_configurar() {
    gpio_init(GPIO_ENTRADA);   // habilito GPIO en el GPIO 1
    gpio_set_dir(GPIO_ENTRADA, GPIO_IN);  // transformo en entrada el puerto GPIO 1
    gpio_pull_down(GPIO_ENTRADA);   // por defecto toma valores 0 si no hay señal
}

int main() {
    stdio_init_all();    // iniciaLIZA UART, USB, etc
    gpio_configurar();   // invoca funcion para inicializar puerto de entrada

    pwm_user_init(0, 10000);   // inicio PWM en GPIO 0

    // Crear semáforo y cola
    sem_flancos = xSemaphoreCreateCounting(10000, 0);   // creo semaforo contador de 10000 posiciones, inicia en posicion 0
    queue_frecuencia = xQueueCreate(5, sizeof(datos_medidos));   // crea cola de 5 posiciones, cada elemento es compatible con estructura datos_medidos 

    // Crear tareas
    xTaskCreate(task_muestreo_gpio, "Muestreo_GPIO", 1024, NULL, 2, NULL);   // usStackDepth: 4KB, \
                                                                                pvParameters: no le pasa ningun valor a ningun argumento, \
                                                                                uxPriority: prioridad maxima, \
                                                                                pxCreatedTask: no se guarda el handler ya que no vamos a controlar la tarea luego de crearla
    xTaskCreate(task_frecuencia, "Frecuencia", 1024, NULL, 1, NULL);
    xTaskCreate(task_imprimir_usb, "Imprimir_USB", 1024, NULL, 1, NULL);

    // Iniciar scheduler
    vTaskStartScheduler();

    while (1);
}
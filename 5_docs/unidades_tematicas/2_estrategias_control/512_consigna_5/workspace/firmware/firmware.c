#include <stdio.h>
#include <string.h>
#include <math.h>
#include "pico/stdlib.h"
#include "hardware/pwm.h"
#include "hardware/clocks.h"

#include "lcd.h"
#include "rtc.h"
#include "ff.h"
#include "diskio.h"
#include "freertos.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

#define IN3 6                           // Dirección 1 puente H
#define IN4 7                           // Dirección 2 puente H
#define ENB 8                           // Enable puente H (PWM)
#define SDA 26                          // Pin SDA display LCD
#define SCL 27                          // Pin SCL display LCD
#define MISO 4                          // Pin MISO del SD
#define CS 1                            // Pin CS del SD
#define SCK 2                           // Pin SCK del SD
#define MOSI 3                          // Pin MOSI del SD
#define SENSOR_IR 18                    // Salida del sensor IR
#define SENTIDO 15                      // Botón para cambio de sentido
#define CALIBRAR 14                     // Botón para ajustar PID
#define CLK 11                          // Pin CLK del encoder
#define DT 10                           // Pin DT del encoder
#define SW 9                            // Pin del botón del encoder

#define T_MUESTREO 120                  // Tiempo de muestreo en ms
#define T_PID 40                        // Tiempo de actualizacion del PID
#define RANURAS 50                      // Cantidad de ranuras en el disco
#define T_ANTIRREBOTE 25                // Tiempo de antirrebote
#define T_PRESIONADO 1000               // Tiempo para mantener presionado el boton del encoder
#define FRECUENCIA 500                  // Frecuencia en Hz del pwm
#define T_PANTALLA 200                  // Tiempo entre actualizaciones del LCD
#define MAX_ARCHIVOS 10000              // Desde data0000.csv hasta data9999.csv

typedef struct {                        // Estructura para la cola del encoder
    bool clk;                           // Guarda el estado de CLK
    bool dt;                            // Guarda el estado de DT
} encoder_t;    

typedef enum {                          // Distintos eventos posibles para la tarea central
    EVENTO_VELOCIDAD,                   // Evento lectura de velocidad
    EVENTO_ENCODER,                     // Evento rotacion encoder
    EVENTO_SW,                          // Evento pulsador encoder
    EVENTO_SENTIDO,                     // Evento boton cambio de sentido
    EVENTO_CALIBRAR,                    // Evento boton calibrar
} evento_t;

typedef struct {                        // Estructura para la cola de la tarea central
    evento_t evento;                    // Evento
    float velocidad;                    // Velocidad en RPM
    int8_t sentido;                     // Sentido del encoder (1 horario, -1 antihorario)
    bool sw;                            // Pulsador del encoder (0 pulsado, 1 mantenido)
} central_t;

typedef struct {                        // Estructura con datos a mostrar
    uint8_t menu;                       // Menu a mostrar
    int16_t velocidad_objetivo;         // Velocidad objetivo (menu 0)
    float velocidad_medida;             // Velocidad medida (menu 0)
    float t_aceleracion;                // Tiempo de aceleracion (menu 1)
    float t_desaceleracion;             // Tiempo de desaceleracion (menu 1)
} configuracion_t;

typedef struct {                        // Estructura para los parametros del PID
    float kp;
    float ki;
    float kd;
} parametros_t;

SemaphoreHandle_t semaforo_contador;    // Contador para velocidad 
SemaphoreHandle_t semaforo_sw;          // Semaforo para el switch del encoder
SemaphoreHandle_t semaforo_sentido;     // Semaforo para el switch de sentido
SemaphoreHandle_t semaforo_calibrar;    // Semaforo para el switch de calibrar
SemaphoreHandle_t semaforo_i2c;         // Semaforo mutex para el I2C
SemaphoreHandle_t semaforo_pulsador;    // Semaforo para indicar pulsacion a la tarea de control
QueueHandle_t cola_encoder;             // Cola para estados del encoder (para sentido)
QueueHandle_t cola_central;             // Cola para comunicarse con la tarea central
QueueHandle_t cola_configuracion;       // Cola con datos de configuracion
QueueHandle_t cola_mensaje_sd;          // Cola con mensajes de la sd para el LCD
QueueHandle_t cola_pid;                 // Cola que envia las constantes del PID 

TaskHandle_t boton_calibrar;            // Handle para borrar la tarea del boton de calibrar despues de la calibracion

/**
 * @brief Interrupción del GPIO, se evaluan todas las interrupciones
 */
void gpio_irq_handler(uint gpio, uint32_t event_mask) {
    BaseType_t to_higher_priority_task = pdFALSE;
    encoder_t encoder;

    switch(gpio) {
        case SENSOR_IR: // IRQ para la velocidad
            xSemaphoreGiveFromISR(semaforo_contador, &to_higher_priority_task);
            break;
        case SW: // IRQ para el boton del encoder
            gpio_set_irq_enabled(SW, event_mask, false);
            xSemaphoreGiveFromISR(semaforo_sw, &to_higher_priority_task);
            break;
        case SENTIDO: // IRQ para el boton sentido
            gpio_set_irq_enabled(SENTIDO, event_mask, false);
            xSemaphoreGiveFromISR(semaforo_sentido, &to_higher_priority_task);
            break;
        case CALIBRAR: // IRQ para el boton calibrar
            gpio_set_irq_enabled(CALIBRAR, event_mask, false);
            xSemaphoreGiveFromISR(semaforo_calibrar, &to_higher_priority_task);
            break;
        case CLK: // IRQ para el CLK del encoder
            encoder.clk = gpio_get(CLK);
            encoder.dt = gpio_get(DT);
            xQueueOverwriteFromISR(cola_encoder, &encoder, &to_higher_priority_task);
            break;
    }

    portYIELD_FROM_ISR(to_higher_priority_task);
}

/**
 * @brief Tarea que calcula la velocidad y la envía por una cola
 * Envia el mensaje de EVENTO_VELOCIDAD y en mensaje.velocidad la velocidad del motor
 */
void task_velocidad(void *params) {
    central_t mensaje;
    uint16_t pulsos;
    TickType_t ultimo_tick = xTaskGetTickCount();

    mensaje.evento = EVENTO_VELOCIDAD;

    while(1) {
        vTaskDelayUntil(&ultimo_tick, pdMS_TO_TICKS(T_MUESTREO)); // Espera el tiempo de muestra

        pulsos = uxSemaphoreGetCount(semaforo_contador); // Leemos los pulsos
        xQueueReset(semaforo_contador); // Vaciamos el contador
        
        mensaje.velocidad = (float)(pulsos * 60000) / (T_MUESTREO * RANURAS); // Calculo de velocidad en RPM

        if(gpio_get(IN3) == 1) mensaje.velocidad = -mensaje.velocidad; // Verificamos el sentido de giro
        
        xQueueSendToFront(cola_central, &mensaje, portMAX_DELAY); // Enviamos al frente para dar prioridad
    }
}

/**
 * @brief Tarea que hacer las rampas y ajusta el PWM (PID)
 * Espera el tiempo de calculo del PID, evalua el set point de acuerdo a la configuracion y aplica el PID
 */
void task_control(void *params) {
    configuracion_t configuracion;
    parametros_t pid;

    configuracion.velocidad_medida = 0;
    configuracion.velocidad_objetivo = 0;
    configuracion.t_aceleracion = 0;
    configuracion.t_desaceleracion = 0;

    uint32_t slice = pwm_gpio_to_slice_num(ENB); // Guarda el slice del pin 
    uint16_t max_dc = 1000000 / FRECUENCIA; // Obtenemos el maximo valor del wrap, con 500Hz: 2000 pasos
    float set_point = 0;
    //float kp = 0.5, ki = 0.6, kd = 0.005; // Valores inicializados para pruebas
    // El mejor: kp = 0.5 o 0.4, ki = 0.6, kd = 0.005;
    float error, error_anterior1 = 0, error_anterior2 = 0, salida_pid, salida_pid_anterior = 0;
    float tm = T_PID / 1000.0;
    int16_t velocidad_objetivo_anterior = 0, velocidad_objetivo_actual = 0;
    bool cambio_sentido = 0;
    float tiempo_aceleracion = 0, tiempo_desaceleracion = 0;
    float pendiente = 0;

    pwm_set_clkdiv(slice, frequency_count_khz(CLOCKS_FC0_SRC_VALUE_CLK_SYS) / 1000.0); // Dividimos el clock y pasa a 1MHz
    pwm_set_wrap(slice, max_dc); // Seteamos el valor del wrap
    pwm_set_gpio_level(ENB, 0); // Empieza apagado
    pwm_set_enabled(slice, true); // Habilita el pwm

    xQueuePeek(cola_pid, &pid, portMAX_DELAY);

    TickType_t ultimo_tick = xTaskGetTickCount();

    while(1) {
        vTaskDelayUntil(&ultimo_tick, pdMS_TO_TICKS(T_PID)); // Espera el tiempo del PID
        xQueuePeek(cola_configuracion, &configuracion, portMAX_DELAY);

        // Verificamos cambio de configuracion y pulsacion del encoder
        if(xSemaphoreTake(semaforo_pulsador, 0) == pdTRUE && configuracion.velocidad_objetivo != velocidad_objetivo_anterior) {
            if(velocidad_objetivo_anterior == 0) { // Si antes estaba parado debemos definir los pines de direccion
                if(configuracion.velocidad_objetivo > 0) { // Horario
                    gpio_put(IN3, 0);
                    gpio_put(IN4, 1);
                }
                else { // Antihorario
                    gpio_put(IN3, 1);
                    gpio_put(IN4, 0);
                }
            }
            // Guardamos nuevos valores 
            velocidad_objetivo_anterior = configuracion.velocidad_objetivo;
            tiempo_aceleracion = configuracion.t_aceleracion * 1000; // Lo pasamos a ms
            tiempo_desaceleracion = configuracion.t_desaceleracion * 1000; // Lo pasamos a ms

            // Verificamos si hay cambio de sentido
            if((set_point > 0 && velocidad_objetivo_anterior < 0) || (set_point < 0 && velocidad_objetivo_anterior > 0)) {
                cambio_sentido = 1; // Hay que hacer cambio de sentido
                velocidad_objetivo_actual = 0; // Primero se hace la rampa descendente
                pendiente = set_point / tiempo_desaceleracion; // para frenar 
            }
            else {
                cambio_sentido = 0; // No hay que cambiar el sentido
                velocidad_objetivo_actual = velocidad_objetivo_anterior; // Vamos directo al objetivo
                // Verificamos si hay que acelerar o desacelerar
                if(fabs((float)velocidad_objetivo_actual) > fabs(set_point)) pendiente = (velocidad_objetivo_actual - set_point) / tiempo_aceleracion;
                else pendiente = (velocidad_objetivo_actual - set_point) / tiempo_desaceleracion;
            }
        }

        // En el caso de no cambiar de sentido solo actualizamos el set_point
        if(cambio_sentido == 0) {
            set_point += T_PID * pendiente; // Actualizamos el set point
            // Verificamos si nos pasamos del valor objetivo
            if(pendiente < 0 && set_point < velocidad_objetivo_actual) set_point = velocidad_objetivo_actual;
            if(pendiente > 0 && set_point > velocidad_objetivo_actual) set_point = velocidad_objetivo_actual;
            
            if(velocidad_objetivo_anterior == 0) { // Verificamos si se planea frenar del todo
                if (fabs(configuracion.velocidad_medida) <= 200 && salida_pid == 0) { // Damos un margen de 200RPM para el frenado
                    // Reseteamos las variables de control y apagamos leds
                    error_anterior1 = 0;
                    error_anterior2 = 0;
                    salida_pid_anterior = 0;
                    set_point = 0;
                    gpio_put(IN3, 1);
                    gpio_put(IN4, 1);
                }
            }
            
        }
        // Si cambiamos el sentido primero tenemos que desacelerar y pasar por cero, despues cambiar el objetivo y pendiente
        if(cambio_sentido == 1) {
            if(set_point != 0) {
                set_point -= T_PID * pendiente;
                if(pendiente < 0 && set_point > velocidad_objetivo_actual) set_point = velocidad_objetivo_actual;
                if(pendiente > 0 && set_point < velocidad_objetivo_actual) set_point = velocidad_objetivo_actual;
            }
            else {
                if (fabs(configuracion.velocidad_medida) <= 200 && salida_pid == 0) {
                    error_anterior1 = 0;
                    error_anterior2 = 0;
                    salida_pid_anterior = 0;
                    velocidad_objetivo_actual = velocidad_objetivo_anterior;
                    cambio_sentido = 0;
                    pendiente = velocidad_objetivo_actual / tiempo_aceleracion;
                    gpio_put(IN3, 1);
                    gpio_put(IN4, 1);
                    // Dejamos 500ms frenado
                    vTaskDelay(pdMS_TO_TICKS(500));
                    if(velocidad_objetivo_actual > 0) {
                        gpio_put(IN3, 0);
                        gpio_put(IN4, 1);
                    }
                    else {
                        gpio_put(IN3, 1);
                        gpio_put(IN4, 0);
                    }
                }
            }
        }

        // Calculamos el error y el PID
        error = fabs(set_point) - fabs(configuracion.velocidad_medida);
        salida_pid = salida_pid_anterior 
                   + (pid.kp + pid.kd / tm) * error 
                   + (-pid.kp + pid.ki * tm - 2 * pid.kd / tm) * error_anterior1 
                   + (pid.kd / tm) * error_anterior2;

        salida_pid_anterior = salida_pid;
        error_anterior2 = error_anterior1;
        error_anterior1 = error;

        // Ajuste por minimo PWM para arrancar
        salida_pid = 350 + 0.825 * salida_pid;

        // Limitacion del pwm, saturacion
        if(salida_pid > 2000) salida_pid = 2000;
        if(salida_pid < 360) salida_pid = 0;

        pwm_set_gpio_level(ENB, (uint16_t) salida_pid);
    }
}

/**
 * @brief Tarea para leer el boton del encoder, tanto presionado como mantenido
 * Envia el mensaje de EVENTO_SW y en mensaje.sw un 0 si se pulso y 1 si se mantuvo
 */
void task_switch(void *params) {
    central_t mensaje;

    mensaje.evento = EVENTO_SW;

    while(1) {
        xSemaphoreTake(semaforo_sw, portMAX_DELAY); // Esperamos el semaforo
        vTaskDelay(pdMS_TO_TICKS(T_ANTIRREBOTE)); // Esperamos el tiempo de rebote

        gpio_set_irq_enabled(SW, GPIO_IRQ_LEVEL_HIGH, true); // Cambio de nivel para soltar el boton

        if(xSemaphoreTake(semaforo_sw, pdMS_TO_TICKS(T_PRESIONADO)) == pdTRUE) { // Esperamos a ver si se suelta el boton
            mensaje.sw = 0; // Se presiono
            xQueueSendToBack(cola_central, &mensaje, portMAX_DELAY);
        }
        else {
            mensaje.sw = 1; // Se mantuvo
            xQueueSendToBack(cola_central, &mensaje, portMAX_DELAY);
            xSemaphoreTake(semaforo_sw, portMAX_DELAY); // Se espera a que suelte el boton 
        }

        vTaskDelay(pdMS_TO_TICKS(T_ANTIRREBOTE)); // Espera el tiempo de rebote

        gpio_set_irq_enabled(SW, GPIO_IRQ_EDGE_FALL, true); // Reactiva el ciclo de interrupcion
    }
}

/**
 * @brief Tarea que lee la rotacion del encoder
 * Envia el mensaje de EVENTO_ENCODER y en mensaje.sentido un 1-horario y -1-antihorario
 */
void task_encoder(void *params) {
    central_t mensaje;
    encoder_t actual, anterior = {0, 0};

    TickType_t ultimo_tick = xTaskGetTickCount(), tick_actual;

    mensaje.evento = EVENTO_ENCODER;

    while (1) {
        xQueueReceive(cola_encoder, &actual, portMAX_DELAY); // Espera la cola del encoder

        tick_actual = xTaskGetTickCount(); // Se verifican los ticks que pasaron

        // Se verifica el rebote comparando el tick anterior con el actual
        if ((tick_actual - ultimo_tick) >= pdMS_TO_TICKS(T_ANTIRREBOTE)) {
            if (actual.clk != anterior.clk) { // Verificamos si cambio del estado del CLK
                if (actual.clk != actual.dt) {
                    mensaje.sentido = 1; // Giro horario
                } 
                else {
                    mensaje.sentido = -1; // Giro antihorario
                }
                ultimo_tick = tick_actual; // Actualizamos el tick
                xQueueSendToBack(cola_central, &mensaje, portMAX_DELAY);
            }
            anterior = actual; // Actualizamos el estado de CLK
        }
    }
}

/**
 * @brief Tarea que controla el flujo de datos del programa
 * Recibe por una cola que evento se activo y en base a eso actualiza la cola de configuracion
 */
void task_central(void *params) {
    central_t mensaje;
    configuracion_t configuracion;

    configuracion.menu = 0;
    configuracion.velocidad_medida = 0;
    configuracion.velocidad_objetivo = 1500;
    configuracion.t_aceleracion = 5;
    configuracion.t_desaceleracion = 5;

    while(1) {
        xQueueReceive(cola_central, &mensaje, portMAX_DELAY); // Esperamos un mensaje

        switch(mensaje.evento) { // Dependiendo del evento actuamos
            case EVENTO_VELOCIDAD: // Actualizo velocidad
                configuracion.velocidad_medida = mensaje.velocidad;
                break;
            case EVENTO_ENCODER: // Actualizo configuraciones
                if(configuracion.menu == 0) {
                    if(mensaje.sentido == 1) {
                        if(configuracion.velocidad_objetivo == -500) configuracion.velocidad_objetivo = 0;
                        else if(configuracion.velocidad_objetivo == 0) configuracion.velocidad_objetivo = 500;
                        else if((configuracion.velocidad_objetivo < 2700 && configuracion.velocidad_objetivo > 0) || configuracion.velocidad_objetivo < -500) {
                            configuracion.velocidad_objetivo += 10;
                        }
                    }
                    else {
                        if(configuracion.velocidad_objetivo == 500) configuracion.velocidad_objetivo = 0;
                        else if(configuracion.velocidad_objetivo == 0) configuracion.velocidad_objetivo = -500;
                        else if(configuracion.velocidad_objetivo > 500 || (configuracion.velocidad_objetivo > -2700 && configuracion.velocidad_objetivo < 0)) {
                            configuracion.velocidad_objetivo -= 10;
                        }
                    }
                }
                else if(configuracion.menu == 1) { // Actualizo tiempo de aceleracion 
                    if(mensaje.sentido == 1) {
                        if(configuracion.t_aceleracion < 99.8) configuracion.t_aceleracion += 0.1;
                    }
                    else {
                        if(configuracion.t_aceleracion > 1.1) configuracion.t_aceleracion -= 0.1;
                    } 
                }
                else { // Actualizo tiempo de desaceleracion 
                    if(mensaje.sentido == 1) {
                        if(configuracion.t_desaceleracion < 99.8) configuracion.t_desaceleracion += 0.1;
                    }
                    else {
                        if(configuracion.t_desaceleracion > 1.1) configuracion.t_desaceleracion -= 0.1;
                    } 
                }
                break;
            case EVENTO_SW:
                if(mensaje.sw == 0) {
                    if(configuracion.menu == 0) {
                        // comienza el control a la velocidad configurada
                        xSemaphoreGive(semaforo_pulsador);
                    }
                    else if(configuracion.menu == 1) configuracion.menu = 2;
                    else configuracion.menu = 1;
                }
                if(mensaje.sw == 1) {
                    if(configuracion.menu == 0) configuracion.menu = 1;
                    else configuracion.menu = 0;
                }
                break;
            case EVENTO_SENTIDO:
                if(configuracion.menu == 0) {
                    if(mensaje.sw == 0) configuracion.velocidad_objetivo = -configuracion.velocidad_objetivo;
                    else configuracion.velocidad_objetivo = 0;
                }
                break;
        }

        xQueueOverwrite(cola_configuracion, &configuracion);
    }
}

/**
 * @brief Tarea que maneja el display LCD
 * Recibe datos de la cola de configuracion o del datalogger
 */
void task_lcd(void *params) {
    configuracion_t configuracion;
    uint8_t menu_anterior = 0, mensaje_sd;
    bool actualizacion_sd = false;
    char numero[6];

    xSemaphoreTake(semaforo_i2c, portMAX_DELAY);
    lcd_set_cursor(0, 0);
    lcd_string("V Set:      RPM");
    lcd_set_cursor(1, 0);
    lcd_string("V Med:      RPM");
    xSemaphoreGive(semaforo_i2c);

    while(1) {
        vTaskDelay(pdMS_TO_TICKS(T_PANTALLA));

        xQueuePeek(cola_configuracion, &configuracion, portMAX_DELAY);

        xSemaphoreTake(semaforo_i2c, portMAX_DELAY);

        if(xQueueReceive(cola_mensaje_sd, &mensaje_sd, 0) == pdTRUE) {
            lcd_clear();
            actualizacion_sd = true;
            switch(mensaje_sd) {
                case 0:
                    lcd_set_cursor(0, 0);
                    lcd_string("Archivo creado");
                    lcd_set_cursor(1, 0);
                    lcd_string("exitosamente");
                    break;
                case 1:
                    lcd_set_cursor(0, 0);
                    lcd_string("Almacenamiento");
                    lcd_set_cursor(1, 0);
                    lcd_string("lleno");
                    break;
                case 2:
                    lcd_set_cursor(0, 0);
                    lcd_string("Error en montaje");
                    lcd_set_cursor(1, 0);
                    lcd_string("Reconecte la SD");
                    break;
                case 3:
                    lcd_set_cursor(0, 0);
                    lcd_string("SD reconectada");
                    break;
                case 4:
                    lcd_set_cursor(0, 0);
                    lcd_string("SD desconectada");
                    break;
            }
            vTaskDelay(3000);
            xQueuePeek(cola_configuracion, &configuracion, portMAX_DELAY);
        }

        if(configuracion.menu != menu_anterior || actualizacion_sd == true) { // Si cambia de menu hay que cambiar la plantilla
            menu_anterior = configuracion.menu;
            actualizacion_sd = false;
            if (menu_anterior == 0) { // Menu para mostrar velocidad
                lcd_clear();
                lcd_set_cursor(0, 0);
                lcd_string("V Set:      RPM");
                lcd_set_cursor(1, 0);
                lcd_string("V Med:      RPM");
            }
            else { // Menu para mostrar tiempos
                lcd_clear();
                lcd_set_cursor(0, 0);
                lcd_string("T Ace:     Seg");
                lcd_set_cursor(1, 0);
                lcd_string("T Des:     Seg");
            }
        }

        if (menu_anterior == 0) { // Actualizamos datos
            lcd_set_cursor(0, 7);
            sprintf(numero, "%5d", configuracion.velocidad_objetivo);
            lcd_string(numero);
            lcd_set_cursor(1, 7);
            sprintf(numero, "%5.0f", configuracion.velocidad_medida);
            lcd_string(numero);
        }
        else {
            lcd_set_cursor(0, 7);
            sprintf(numero, "%4.1f", configuracion.t_aceleracion);
            lcd_string(numero);
            lcd_set_cursor(1, 7);
            sprintf(numero, "%4.1f", configuracion.t_desaceleracion);
            lcd_string(numero);
        }

        xSemaphoreGive(semaforo_i2c);
    }
}

/**
 * @brief Tarea que lee el boton de cambio de sentido
 */
void task_sentido(void *params) {
    central_t mensaje;

    mensaje.evento = EVENTO_SENTIDO;

    while(1) {
        xSemaphoreTake(semaforo_sentido, portMAX_DELAY); // Esperamos el semaforo
        vTaskDelay(pdMS_TO_TICKS(T_ANTIRREBOTE)); // Esperamos el tiempo de rebote

        gpio_set_irq_enabled(SENTIDO, GPIO_IRQ_LEVEL_HIGH, true); // Cambio de nivel para soltar el boton

        if(xSemaphoreTake(semaforo_sentido, pdMS_TO_TICKS(T_PRESIONADO)) == pdTRUE) { // Esperamos a ver si se suelta el boton
            mensaje.sw = 0; // Se presionó (Cambio de sentido)
            xQueueSendToBack(cola_central, &mensaje, portMAX_DELAY);
        }
        else {
            mensaje.sw = 1; // Se mantuvo (Velocidad a cero)
            xQueueSendToBack(cola_central, &mensaje, portMAX_DELAY);
            xSemaphoreTake(semaforo_sentido, portMAX_DELAY); // Se espera a que suelte el boton 
        }

        vTaskDelay(pdMS_TO_TICKS(T_ANTIRREBOTE)); // Espera el tiempo de rebote

        gpio_set_irq_enabled(SENTIDO, GPIO_IRQ_EDGE_FALL, true); // Reactiva el ciclo de interrupcion
    }
}

/**
 * @brief Tarea que lee el boton de calibrar
 */
void task_calibrar(void *params) {
    central_t mensaje;

    mensaje.evento = EVENTO_CALIBRAR;

    while(1) {
        xSemaphoreTake(semaforo_calibrar, portMAX_DELAY); // Esperamos el semaforo
        vTaskDelay(pdMS_TO_TICKS(T_ANTIRREBOTE)); // Esperamos el tiempo de rebote

        gpio_set_irq_enabled(CALIBRAR, GPIO_IRQ_LEVEL_HIGH, true);

        if(xSemaphoreTake(semaforo_calibrar, pdMS_TO_TICKS(T_PRESIONADO)) == pdTRUE) { // Esperamos a ver si se suelta el boton
            mensaje.sw = 0; // Se presionó
            xQueueSendToBack(cola_central, &mensaje, portMAX_DELAY);
        }
        else {
            mensaje.sw = 1; // Se mantuvo
            xQueueSendToBack(cola_central, &mensaje, portMAX_DELAY);
            xSemaphoreTake(semaforo_calibrar, portMAX_DELAY); // Se espera a que suelte el boton 
        }

        vTaskDelay(pdMS_TO_TICKS(T_ANTIRREBOTE)); // Espera el tiempo de rebote

        gpio_set_irq_enabled(CALIBRAR, GPIO_IRQ_EDGE_FALL, true); // Reactiva el ciclo de interrupcion
    }
}

/**
 * @brief Tarea que maneja el sistema de archivos, guarda de a 5 datos
 * Tiene verificaciones de conexion, desconexion, de error de creacion
 */
void task_datalogger(void *params) {
    time_t horas[5]; // Para almacenar 10 horas distintas 
    configuracion_t configuracion; // Para obtener la velocidad
    parametros_t pid; // Para guardar las constantes del PID
    float velocidades[5]; // Para almacenar 10 velocidades distintas
    float frecuencias[5]; // Para almacenar 10 frecuencias del sensor distintas
    char sentido[5]; // Para guardar 10 caracteres que indican sentido (H o A)
    float dc[5]; // Para almacenas 10 ciclos de actividad distintos

    uint canal = pwm_gpio_to_channel(ENB); // Canal de pwm usado
    uint slice_usado = pwm_gpio_to_slice_num(ENB); // Slice de pwm usado
    uint32_t nivel_pwm; // Nivel de pwm usado

    FATFS fs; // Objeto sistema de archivos
    FIL file; // Objeto archivo
    FRESULT resultado; // Resultado FatFs
    UINT bw; // Bytes escritos

    uint8_t mensaje_sd, datos = 0;
    uint16_t i = 0;
    bool sd = false; // Flag para identificar la presencia de tarjeta SD
    bool archivo = false; // Flag para identificar si ya se creó el archivo del encendido
    bool archivo_pid = false; // Flag para saber si se guardaron las constantes del PID
    bool error_mostrado = false;
    char nombre_archivo[12]; // Nombre del archivo creado
    char buffer[38];

    xQueuePeek(cola_pid, &pid, portMAX_DELAY);

    TickType_t ultimo_tick = xTaskGetTickCount();

    while(1) {
        vTaskDelayUntil(&ultimo_tick, pdMS_TO_TICKS(1000));
        xQueuePeek(cola_configuracion, &configuracion, portMAX_DELAY);

        if(sd == false) { // Si no se cargo una SD
            resultado = f_mount(&fs, "", 1); // Trata de montar SD
            if(resultado == FR_OK) { // Si se logra
                error_mostrado = false; // Desactivamos el mostrado de error
                sd = true; // Indicamos que se leyo la sd
                datos = 0; // Reseteamos el indice de los datos guardados

                if(archivo == true) { // En caso de que ya se haya creado el archivo verificamos si se cambió de SD
                    resultado = f_open(&file, nombre_archivo, FA_WRITE | FA_OPEN_EXISTING); // Tratamos de abrir para agregar más datos
                    if(resultado == FR_OK) { // Se reconectó la SD
                        mensaje_sd = 3;
                        xQueueSendToBack(cola_mensaje_sd, &mensaje_sd, portMAX_DELAY);
                    }
                    else { // Se conectó otra SD
                        resultado = f_lseek(&file, f_size(&file));
                        archivo = false; 
                        archivo_pid = false;
                    }
                }

                if(archivo_pid == false) { // Si no se creo el archivo de datos de PID lo creamos
                    archivo_pid = true;
                    resultado = f_open(&file, "pid.txt", FA_OPEN_ALWAYS | FA_WRITE);
                    resultado = f_lseek(&file, f_size(&file));
                    sprintf(buffer, "kp = %.1f; ki = %.1f; kd = %.3f\n", pid.kp, pid.ki, pid.kd);
                    resultado = f_write(&file, buffer, strlen(buffer), &bw);
                    f_close(&file);
                }

                if(archivo == false) { // Si nunca se creó el archivo vamos a identificarlo
                    for(i = 0; i < MAX_ARCHIVOS; i++) {
                        sprintf(nombre_archivo, "data%04d.csv", i); // Creamos el nombre del archivo
                        resultado = f_open(&file, nombre_archivo, FA_READ); // Tratamos de leerlo
                        if(resultado == FR_NO_FILE) { // En caso de no leerlo nos indica cual nombre toca
                            resultado = f_open(&file, nombre_archivo, FA_WRITE | FA_CREATE_ALWAYS); // Lo creamos
                            sprintf(buffer, "Fecha;Hora;Vel;Frec;DC;Sentido\n");
                            resultado = f_write(&file, buffer, strlen(buffer), &bw);
                            archivo = true; // Inicamos que se creó
                            mensaje_sd = 0;
                            xQueueSendToBack(cola_mensaje_sd, &mensaje_sd, portMAX_DELAY);
                            f_close(&file);
                            break;
                        }
                        else {
                            f_close(&file); // Si se pudo leer cerramos el archivo
                        }
                    }
                    if(error_mostrado == false && i == MAX_ARCHIVOS) { // Tarjeta SD llena
                        error_mostrado = true;
                        mensaje_sd = 1;
                        xQueueSendToBack(cola_mensaje_sd, &mensaje_sd, portMAX_DELAY);
                    }
                }
            }
            else if(error_mostrado == false) { // Error en montaje
                error_mostrado = true;
                mensaje_sd = 2;
                xQueueSendToBack(cola_mensaje_sd, &mensaje_sd, portMAX_DELAY);
            }
        }
        else { // Ya esta conectada la SD
            xSemaphoreTake(semaforo_i2c, portMAX_DELAY);
            horas[datos] = rtc_get_time(); // Obtenemos hora y fecha
            xSemaphoreGive(semaforo_i2c);
            velocidades[datos] = configuracion.velocidad_medida; // Guardamos la velocidad actual
            frecuencias[datos] = configuracion.velocidad_medida * RANURAS / 120.0; // Calculamos la frecuencia del sensor
            // Obtenemos el nivel del PWM en base al registro ya que no tenia funcion en el SDK
            if(canal == 0) nivel_pwm = pwm_hw->slice[slice_usado].cc & 0xFFFF;
            else nivel_pwm = (pwm_hw->slice[slice_usado].cc >> 16) & 0xFFFF;
            dc[datos] = nivel_pwm * 100.0 / 2000.0; // Calculamos el ciclo de actividad con el nivel del PWM
            // Verificamos el sentido o si esta parado
            if(gpio_get(IN3) == 1) {
                if(gpio_get(IN4) == 1) sentido[datos] = 'P';
                else sentido[datos] = 'A';
            }
            else sentido[datos] = 'H';
            // Verificamos cambio de hora
            if(datos > 0) {
                if(horas[datos].second != horas[datos - 1].second) datos++;
            }
            if(datos == 0) {
                if(horas[datos].second != horas[4].second) datos++;
            }
            // Si se llena el buffer hay que guardar
            if(datos == 5) {
                resultado = f_open(&file, nombre_archivo, FA_WRITE | FA_OPEN_APPEND);
                if(resultado != FR_OK) { // Si no se pudo abrir indicamos que se desconecto la SD
                    mensaje_sd = 4;
                    xQueueSendToBack(cola_mensaje_sd, &mensaje_sd, portMAX_DELAY);
                    sd = false;
                    error_mostrado = true;
                    datos = 0;
                }
                else { // Se pudo abrir el archivo
                    for(datos = 0; datos < 5; datos++) { 
                        //Guardamos los 5 datos
                        sprintf(buffer,"%02d/%02d/%04d;%02d:%02d:%02d;%4.0f;%6.1f;%3.1f;%c\n",
                            horas[datos].date, horas[datos].month, horas[datos].year,
                            horas[datos].hour, horas[datos].minute, horas[datos].second,
                            velocidades[datos], frecuencias[datos], dc[datos], sentido[datos]);
                        resultado = f_write(&file, buffer, strlen(buffer), &bw);
                    }
                    f_close(&file); // Cerramos el archivo
                }
                datos = 0;
            }
        }
    }
}

/**
 * @brief Tarea para ajustar PID por grilla de valores
 * En base a valores obtenidos experimentalmente verificamos la mejor combinacion 
 */
void task_tune(void *params) {
    configuracion_t velocidad;
    parametros_t parametros_finales;
    float kp[] = {0.4, 0.5, 0.6};
    float ki[] = {0.6, 0.7, 0.8, 0.9};
    float kd[] = {0.004, 0.005, 0.006};
    float error, error_anterior1 = 0, error_anterior2 = 0, error_total = 0, menor_error = 999999;
    float salida_pid, salida_pid_anterior = 0;
    float tm = T_PID / 1000.0;
    TickType_t ultimo_tick = xTaskGetTickCount();

    lcd_clear();
    lcd_set_cursor(0, 0);
    lcd_string("Calibrando");
    
    for(uint8_t i = 0; i < 3; i++) { // Recorremos los kp
        for(uint8_t j = 0; j < 4; j++) { // Recorremos los ki
            for(uint8_t k = 0; k < 3; k++) { // Recorremos los kd
                // Establecemos el sentido
                gpio_put(IN3, 0); 
                gpio_put(IN4, 1); 
                for(uint8_t t = 0; t < 42; t++) { // Aplicamos el PID durante 42 ciclos
                    vTaskDelayUntil(&ultimo_tick, pdMS_TO_TICKS(T_PID));
                    xQueuePeek(cola_configuracion, &velocidad, portMAX_DELAY);
                    error = 1500 - fabs(velocidad.velocidad_medida);
                    error_total += fabs(error); // Acumulamos el error total
                    salida_pid = salida_pid_anterior 
                                + (kp[i] + kd[k] / tm) * error 
                                + (-kp[i] + ki[j] * tm - 2 * kd[k] / tm) * error_anterior1 
                                + (kd[k] / tm) * error_anterior2;
                    salida_pid_anterior = salida_pid;
                    error_anterior2 = error_anterior1;
                    error_anterior1 = error;
                    // Ajuste por minimo PWM para arrancar
                    salida_pid = 350 + 0.825 * salida_pid;
                    // Limitacion del pwm, saturacion
                    if(salida_pid > 2000) salida_pid = 2000;
                    if(salida_pid < 360) salida_pid = 0;
                    pwm_set_gpio_level(ENB, (uint16_t) salida_pid);                
                }
                pwm_set_gpio_level(ENB, 0);  
                gpio_put(IN3, 1);
                gpio_put(IN4, 1); 
                vTaskDelay(300); // Esperamos a que se frene
                salida_pid_anterior = 0;
                error_anterior1 = 0;
                error_anterior2 = 0;
                if(error_total < menor_error) {
                    menor_error = error_total;
                    parametros_finales.kp = kp[i];
                    parametros_finales.ki = ki[j];
                    parametros_finales.kd = kd[k];
                } 
                error_total = 0;
            }
        }
    }
    xQueueSendToBack(cola_pid, &parametros_finales, portMAX_DELAY); // Enviamos los parametros a control
    
    // Activamos el resto de interrupciones
    gpio_set_irq_enabled(SENTIDO, GPIO_IRQ_EDGE_FALL, true);
    gpio_set_irq_enabled(SW, GPIO_IRQ_EDGE_FALL, true);

    // Creamos el resto de tareas
    xTaskCreate(
        task_switch,
        "task_switch",
        configMINIMAL_STACK_SIZE,
        NULL,
        tskIDLE_PRIORITY + 2,
        NULL
    );

    xTaskCreate(
        task_lcd,
        "task_lcd",
        configMINIMAL_STACK_SIZE * 2,
        NULL,
        tskIDLE_PRIORITY + 1,
        NULL
    );
                
    xTaskCreate(
        task_sentido,
        "task_sentido",
        configMINIMAL_STACK_SIZE,
        NULL,
        tskIDLE_PRIORITY + 1,
        NULL
    );
                
    xTaskCreate(
        task_datalogger,
        "task_datalogger",
        configMINIMAL_STACK_SIZE * 4,
        NULL,
        tskIDLE_PRIORITY +1,
        NULL
    );

    vTaskDelete(NULL);
}

/**
 * @brief Tarea para setear hora
 * Inicia mostrando la hora y permite su modificacion, luego se procede a la calibracion
 */
void task_seteo(void *params) {
    central_t encoder;
    time_t hora;
    bool cambiar_hora = 0;
    char fecha[9];
    uint8_t unidad = 0;

    while(1) {
        if(cambiar_hora == 0) xQueueReceive(cola_central, &encoder, pdMS_TO_TICKS(100));
        else xQueueReceive(cola_central, &encoder, portMAX_DELAY);

        if(encoder.evento == EVENTO_CALIBRAR) { 
            if(encoder.sw == 0) { // Si se pulso activamos la configuracoin y cambiamos entre valores
                cambiar_hora = 1;
                unidad++;
                if(unidad == 7) unidad = 1;
            }
            else {
                if(cambiar_hora == 1) {
                    rtc_set_time(hora);
                }

                // Activamos la interrupcion para leer la velocidad
                gpio_set_irq_enabled(SENSOR_IR, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true);

                // Creamos las tareas de lectura de velocidad, control, central y de tuneo
                xTaskCreate(
                    task_velocidad, 
                    "task_velocidad", 
                    configMINIMAL_STACK_SIZE, 
                    NULL, 
                    tskIDLE_PRIORITY + 2, 
                    NULL
                );
                
                xTaskCreate(
                    task_control,
                    "task_control",
                    configMINIMAL_STACK_SIZE * 2,
                    NULL,
                    tskIDLE_PRIORITY + 2,
                    NULL
                );
                
                xTaskCreate(
                    task_central, 
                    "task_central", 
                    configMINIMAL_STACK_SIZE, 
                    NULL, 
                    tskIDLE_PRIORITY + 3, 
                    NULL
                );

                xTaskCreate(
                    task_tune,
                    "task_tune",
                    configMINIMAL_STACK_SIZE * 2,
                    NULL,
                    tskIDLE_PRIORITY + 2,
                    NULL
                );

                // Inhabilitamos la interrupcion del boton calibrar, borramos su tarea y esta tarea tambien
                gpio_set_irq_enabled(CALIBRAR, GPIO_IRQ_EDGE_FALL | GPIO_IRQ_LEVEL_HIGH, false);
                vTaskDelete(boton_calibrar);
                vTaskDelete(NULL);
            }
        }

        if(cambiar_hora == 0) { // Obtenemos la hora
            hora = rtc_get_time();
        }
        else { // Cambiamos la hora
            if(encoder.evento == EVENTO_ENCODER) {
                switch(unidad) {
                    case 1:
                        if(encoder.sentido == 1) {
                            hora.date++;
                            if(hora.date == 32) hora.date = 1;
                        }
                        else {
                            hora.date--;
                            if(hora.date == 0) hora.date = 31;
                        }
                        break;
                    case 2:
                        if(encoder.sentido == 1) {
                            hora.month++;
                            if(hora.month == 13) hora.month = 1;
                        }
                        else {
                            hora.month--;
                            if(hora.month == 0) hora.month = 12;
                        }
                        break;
                    case 3:
                        if(encoder.sentido == 1) {
                            hora.year++;
                            if(hora.year == YEAR_BASE + 100) hora.year = YEAR_BASE;
                        }
                        else {
                            hora.year--;
                            if(hora.year == YEAR_BASE - 1) hora.year = YEAR_BASE + 99;
                        }
                        break;
                    case 4:
                        if(encoder.sentido == 1) {
                            hora.hour++;
                            if(hora.hour == 24) hora.hour = 0;
                        }
                        else {
                            if(hora.hour == 0) hora.hour = 23;
                            else hora.hour--;
                        }
                        break;
                    case 5:
                        if(encoder.sentido == 1) {
                            hora.minute++;
                            if(hora.minute == 60) hora.minute = 0;
                        }
                        else {
                            if(hora.minute == 0) hora.minute = 59;
                            else hora.minute--;
                        }
                        break;
                    case 6:
                        if(encoder.sentido == 1) {
                            hora.second++;
                            if(hora.second == 60) hora.second = 0;
                        }
                        else {
                            if(hora.second == 0) hora.second = 59;
                            else hora.second--;
                        }
                        break;
                }
            }
        }
        
        // Mostramos la hora medida o modificada
        lcd_set_cursor(0, 0);
        sprintf(fecha, "%02d/%02d/%02d", hora.date, hora.month, hora.year - 2000);
        lcd_string(fecha);
        lcd_set_cursor(1, 0);
        sprintf(fecha, "%02d:%02d:%02d", hora.hour, hora.minute, hora.second);
        lcd_string(fecha);
    }
}

int main() {
    stdio_init_all();

    // Inicializacion pin sensor infrarrojo
    gpio_init(SENSOR_IR);
    gpio_set_dir(SENSOR_IR, GPIO_IN);

    // Inicializacion pin CLK del encoder
    gpio_init(CLK);
    gpio_set_dir(CLK, GPIO_IN);

    // Inicializacion pin DT del encoder
    gpio_init(DT);
    gpio_set_dir(DT, GPIO_IN);

    // Inicializacion pin SW del encoder
    gpio_init(SW);
    gpio_set_dir(SW, GPIO_IN);
    gpio_pull_up(SW);

    // Inicializacion pin boton sentido
    gpio_init(SENTIDO);
    gpio_set_dir(SENTIDO, GPIO_IN);
    gpio_pull_up(SENTIDO);

    // Inicializacion pin boton calibrar
    gpio_init(CALIBRAR);
    gpio_set_dir(CALIBRAR, GPIO_IN);
    gpio_pull_up(CALIBRAR);

    // Inicializacion pin IN3 del puente H
    gpio_init(IN3);
    gpio_set_dir(IN3, GPIO_OUT);
    gpio_put(IN3, 1); // Sentido 1
    
    // Inicializacion pin IN4 del puente H
    gpio_init(IN4);
    gpio_set_dir(IN4, GPIO_OUT);
    gpio_put(IN4, 1); // Sentido 1
    
    // Seteo la funcion del PWM
    gpio_set_function(ENB, GPIO_FUNC_PWM);

    // Inicio LCD
    // Inicializo el I2C con un clock de 100 KHz
    i2c_init(i2c1, 100000);
    // Habilito la funcion de I2C en los GPIOs
    gpio_set_function(SDA, GPIO_FUNC_I2C);
    gpio_set_function(SCL, GPIO_FUNC_I2C);
    // Habilito pull-ups
    gpio_pull_up(SDA);
    gpio_pull_up(SCL);
    // Inicializo LCD
    lcd_init(i2c1, 0x27);
    // Inicio RTC
    rtc_init(i2c1);

    // Habilitacion de interrupciones GPIO
    gpio_set_irq_enabled_with_callback(CLK, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true, gpio_irq_handler);
    gpio_set_irq_enabled(CALIBRAR, GPIO_IRQ_EDGE_FALL, true);

    // Creacion de semaforos y colas
    semaforo_sw = xSemaphoreCreateBinary();
    semaforo_sentido = xSemaphoreCreateBinary();
    semaforo_calibrar = xSemaphoreCreateBinary();
    semaforo_i2c = xSemaphoreCreateMutex();
    semaforo_pulsador = xSemaphoreCreateBinary();
    semaforo_contador = xSemaphoreCreateCounting(400, 0);
    cola_encoder = xQueueCreate(1, sizeof(encoder_t));
    cola_central = xQueueCreate(10, sizeof(central_t));
    cola_configuracion = xQueueCreate(1, sizeof(configuracion_t));
    cola_mensaje_sd = xQueueCreate(2, sizeof(uint8_t));
    cola_pid = xQueueCreate(1, sizeof(parametros_t));

    // Creacion de tareas
    xTaskCreate(
        task_encoder,
        "task_encoder",
        configMINIMAL_STACK_SIZE,
        NULL,
        tskIDLE_PRIORITY + 1,
        NULL
    );

    xTaskCreate(
        task_calibrar,
        "task_calibrar",
        configMINIMAL_STACK_SIZE,
        NULL,
        tskIDLE_PRIORITY + 1,
        &boton_calibrar
    );

    xTaskCreate(
        task_seteo,
        "task_seteo",
        configMINIMAL_STACK_SIZE,
        NULL,
        tskIDLE_PRIORITY + 1,
        NULL
    );
    
    // Inicio del Scheduler
    vTaskStartScheduler();

    while(1);
}

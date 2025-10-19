#include <stdio.h>
#include <string.h>
#include <math.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/gpio.h"
#include "hardware/pwm.h"

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

#include "lcd.h"
#include "helper.h"
#include "rtc.h"
#include "as5600.h"
#include "teclado.h"

// Pines
#define I2C_PORT i2c0
#define SDA 4
#define SCL 5
#define IN1 0
#define IN2 1
#define ENA 2
#define LED_VERDE 21
#define LED_ROJO 22

//Direcciones
#define LCD_ADD 0x27
#define RTC_ADD 0x68

// Teclado
const uint FILA_PINS[] = {8, 9, 10, 11};
const uint COL_PINS[]  = {12, 13, 14, 15};

const char teclas[4][4] = {
    {'1', '2', '3', 'A'},
    {'4', '5', '6', 'B'},
    {'7', '8', '9', 'C'},
    {'*', '0', '#', 'D'}
};

#define PWM_FRECUENCIA 2000
#define MUESTREOMS 10


//Estructuras Dispositivos
typedef struct {
    char linea1[16];
    char linea2[16];
} lcd_msg_t;

typedef enum {
    DATALOGGER_SAVE_CONFIG,
    DATALOGGER_SAVE_RESULT,
    DATALOGGER_LOAD_CONFIG,
    DATALOGGER_LOAD_RESULT
} datalogger_cmd_t;

typedef struct {
    datalogger_cmd_t cmd;
    void *data; // configuracion_t* o resultado_t*
    SemaphoreHandle_t done;
} datalogger_msg_t;

typedef enum {
    I2C_LCD,
    I2C_ENCODER,
    I2C_RTC,
    I2C_EEPROM_SAVE_CONF,
    I2C_EEPROM_SAVE_RESULT,
    I2C_EEPROM_LOAD_CONF,
    I2C_EEPROM_LOAD_RESULT
} i2c_dev_t;

typedef struct {
    i2c_dev_t dispositivo;
    void *data_in;    // Datos a enviar (si aplica)
    void *data_out;   // Buffer para leer datos (si aplica)
    SemaphoreHandle_t done; // Semáforo para esperar respuesta
} i2c_guard_t;

// QueueHandlers
QueueHandle_t q_lcd;
QueueHandle_t q_angulo;
QueueHandle_t q_pid;
QueueHandle_t q_datalogger;
QueueHandle_t q_i2c_guard;
QueueHandle_t q_pid_enable;

//Configuracion base
configuracion_t configuracion_actual = {90.0f, 0.0f, 0};

// Prototipo Funciones
void set_motor_pwm(uint8_t gpio);
void motor_sentido_antihorario();
void motor_sentido_horario();

//Tareas

void task_i2c_guard(void *params) {
    i2c_guard_t req;

    while (1) {
        if (xQueueReceive(q_i2c_guard, &req, portMAX_DELAY)) {
            switch (req.dispositivo) {
                case I2C_LCD:
                    lcd_msg_t *msg = (lcd_msg_t*)req.data_in;
                    lcd_set_cursor(0, 0); lcd_string(msg->linea1);
                    lcd_set_cursor(1, 0); lcd_string(msg->linea2);
                    break;
                case I2C_ENCODER:
                    float *ang = (float*)req.data_out;
                    *ang = (float)get_angle_position(i2c0);  // Usa funciones internas del as5600.c
                    break;
                case I2C_RTC:
                    time_t *t = (time_t*)req.data_out;
                    rtc_get_time(I2C_PORT, t);
                    break;
                case I2C_EEPROM_SAVE_CONF:
                    guardar_configuracion((configuracion_t*)req.data_in);
                    break;
                case I2C_EEPROM_SAVE_RESULT:
                    guardar_resultado((resultado_t*)req.data_in);
                    break;
                case I2C_EEPROM_LOAD_CONF:
                    leer_ultima_configuracion((configuracion_t*)req.data_out);
                    break;
                case I2C_EEPROM_LOAD_RESULT:
                    leer_ultimo_resultado((resultado_t*)req.data_out);
                    break;
            }
            if (req.done)
                xSemaphoreGive(req.done);
        }
    }
}

// Task: LCD
void task_lcd(void *params) {
    lcd_msg_t msg;
    while (1) {
        if (xQueueReceive(q_lcd, &msg, portMAX_DELAY)) {
            i2c_guard_t req = {
                .dispositivo = I2C_LCD,
                .data_in = &msg,
                .done = NULL
            };
            xQueueSend(q_i2c_guard, &req, portMAX_DELAY);
        }
    }
}


void task_encoder(void *params) {
    while (1) {
        float ang = 0;
        SemaphoreHandle_t sem = xSemaphoreCreateBinary();
        i2c_guard_t req = {
            .dispositivo = I2C_ENCODER,
            .data_out = &ang,
            .done = sem
        };
        xQueueSend(q_i2c_guard, &req, portMAX_DELAY);
        xSemaphoreTake(sem, portMAX_DELAY);
        vSemaphoreDelete(sem);

        xQueueOverwrite(q_angulo, &ang);
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}


// Task: PID
void task_control_pid(void *params) {
    // Parámetros PID
    float kp = 18.0f;  // Ganancia proporcional
    float ki = 0.2f;  // Ganancia integral
    float kd = 0.1f;  // Ganancia derivativa
    float Ts = MUESTREOMS / 1000.0f; // Periodo de muestreo
    float error_prev = 0.0f;
    float integral = 0.0f;
    bool pid_enable = false;
    float ref = 0.0f;
    float ang = 0, salida = 0;
    float coef_velocidad = 0;
    float vel_max = 5.0f; // Movimiento rapido, 0.5s para 180 grados
    float vel_min = 0.18f; // Movimiento lento, 10s para 180 grados

    while (1) {
        // Chequea si hay señal para habilitar el PID
        bool cmd;
        if (xQueuePeek(q_pid_enable, &cmd, 0)) {
            pid_enable = cmd;
        }
        if (!pid_enable) {
            pwm_set_gpio_level(ENA, 0);
            vTaskDelay(pdMS_TO_TICKS(MUESTREOMS));
            continue;
        }

        xQueuePeek(q_angulo, &ang, 0);
        if (configuracion_actual.tipo_entrada == 0) {
            ref = configuracion_actual.setpoint;
        } else {
            coef_velocidad = configuracion_actual.pendiente; // Valor de 1 a 100
            ref += vel_min + (coef_velocidad - 1.0f) * (vel_max - vel_min) / 99.0f;
            if (ref > configuracion_actual.setpoint) 
            ref = configuracion_actual.setpoint;
        }

        float error = ref - ang;

        // "Wrap" de error a rango [-180, +180] grados
        if (error > 180.0f)
            error -= 360.0f;
        else if (error < -180.0f)
            error += 360.0f;

        // Componente integral (con anti-windup)
        integral += error * Ts;
        
        if(integral >= 100)
        integral = 100;
        if(integral <= -100)
        integral = -100;

        // Componente derivativa
        float derivada = (error - error_prev) / Ts;

        // Salida PID
        salida = (kp * error) + (ki * integral) + (kd * derivada);

        // Actualizar error previo
        error_prev = error;

        salida = fmaxf(fminf(salida, 5000), -5000);

        if (salida > 0) {
            motor_sentido_horario();        // Clockwise
            pwm_set_gpio_level(ENA, (uint16_t) salida);
        } else if (salida < 0) {
            motor_sentido_antihorario();    // Counterclockwise
            pwm_set_gpio_level(ENA, (uint16_t) -salida);
        }  

        xQueueOverwrite(q_pid, &salida);
        vTaskDelay(pdMS_TO_TICKS(MUESTREOMS));
    }
}

// Task: Flags (LEDs)
void task_flags(void *params) {
    while (1) {
        float ang;
        xQueuePeek(q_angulo, &ang, 0);
        bool flag = fabsf(fmodf((configuracion_actual.setpoint - ang + 180.0f), 360.0f) - 180.0f) <= 5.0f;
        gpio_put(LED_VERDE, flag);
        gpio_put(LED_ROJO, !flag);
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

// Task: Datalogger
void task_datalogger(void *params) {
    datalogger_msg_t reqdl;

    while (1) {
        if (xQueueReceive(q_datalogger, &reqdl, portMAX_DELAY)) {
            // Operaciones EEPROM según comando
            SemaphoreHandle_t sem = xSemaphoreCreateBinary();
            i2c_guard_t req;

            switch (reqdl.cmd) {
                case DATALOGGER_SAVE_CONFIG:
                    req.dispositivo = I2C_EEPROM_SAVE_CONF;
                    req.data_in = reqdl.data;
                    req.done = sem;
                    break;

                case DATALOGGER_SAVE_RESULT:
                    req.dispositivo = I2C_EEPROM_SAVE_RESULT;
                    req.data_in = reqdl.data;
                    req.done = sem;
                    break;

                case DATALOGGER_LOAD_CONFIG:
                    req.dispositivo = I2C_EEPROM_LOAD_CONF;
                    req.data_out = reqdl.data;
                    req.done = sem;
                    break;

                case DATALOGGER_LOAD_RESULT:
                    req.dispositivo = I2C_EEPROM_LOAD_RESULT;
                    req.data_out = reqdl.data;
                    req.done = sem;
                    break;
            }
            xQueueSend(q_i2c_guard, &req, portMAX_DELAY);
            xSemaphoreTake(sem, portMAX_DELAY);
            vSemaphoreDelete(sem);
            // Notificar a quien envió el comando que ya terminamos
            if (reqdl.done) {
                xSemaphoreGive(reqdl.done);
            }
        }
    }
}


// Task: Teclado
void task_keyboard(void *params) {
    static char estado_menu = 0;
    static float valorset = 0.0f;
    static float valorpend = 0.0f;
    static int digitsp = 0;
    static int digitpend = 0;
    bool enable = 0;
    float ang;
    float err;
    bool flag_led;
    resultado_t res;

    while (1) {
            char tecla = escanear_teclado();
            lcd_msg_t msg;
            if (estado_menu == 0) {
                switch (tecla) {
                    case 'A':
                        snprintf(msg.linea1, 16, "Tipo de Salida:");
                        snprintf(msg.linea2, 16, "1:Esc 2:Ram    ");
                        xQueueSend(q_lcd, &msg, 0);
                        estado_menu = 1;
                        break;
                    case 'B':
                        //Entra en estado monitor hasta que se presione una tecla
                        while (1) {
                            ang;
                            xQueuePeek(q_angulo, &ang, 0);
                            err = fabsf(fmodf((configuracion_actual.setpoint - ang + 180.0f), 360.0f) - 180.0f);
                            snprintf(msg.linea1, 16, "SP:%3.0f VA:%3.0f  ", configuracion_actual.setpoint, ang);
                            snprintf(msg.linea2, 16, "Err:%3.0f TS:%c   ", err, configuracion_actual.tipo_entrada ? 'R' : 'E');
                            xQueueSend(q_lcd, &msg, 0);
                            char salida = escanear_teclado();
                            if (salida != '\0') {
                            break; // salir del modo monitor
                            }
                            
                        }
                        break;
                    case 'C':
                        snprintf(msg.linea1, 16, "Salida:%c       ", configuracion_actual.tipo_entrada ? 'R' : 'E');
                        snprintf(msg.linea2, 16, "SP:%3.0f P:%3.0f   ", configuracion_actual.setpoint, configuracion_actual.pendiente);
                        xQueueSend(q_lcd, &msg, 0);
                        break;
                    case 'D':
                        // Envia a datalogger, carga resultados
                        SemaphoreHandle_t sem_load = xSemaphoreCreateBinary();
                        datalogger_msg_t reqdlload = {
                            .cmd = DATALOGGER_LOAD_RESULT,
                            .data = &res,
                            .done = sem_load
                        };
                        xQueueSend(q_datalogger, &reqdlload, portMAX_DELAY);
                        xSemaphoreTake(sem_load, portMAX_DELAY); // esperar que terminen
                        vSemaphoreDelete(sem_load);
                        snprintf(msg.linea1, 16, "SP:%3.0f OUT:%3.0f ", res.setpoint, res.salida_control);
                        snprintf(msg.linea2, 16, "Ang:%3.0f F:%02d:%02d", res.angulo, res.fecha.hour, res.fecha.minute);
                        xQueueSend(q_lcd, &msg, 0);
                        break;
                    case '#': 
                        enable = true;
                        xQueueOverwrite(q_pid_enable, &enable);
                        snprintf(msg.linea1, 16, "PID Activado   ");
                        snprintf(msg.linea2, 16, "               ");
                        xQueueSend(q_lcd, &msg, 0);
                        break;
                    case '*': 
                        enable = false;
                        xQueueOverwrite(q_pid_enable, &enable);
                        xQueuePeek(q_angulo, &res.angulo, 0);
                        res.setpoint = configuracion_actual.setpoint;
                        xQueuePeek(q_pid, &res.salida_control, 0);
                        res.tipo_entrada = configuracion_actual.tipo_entrada;
                        if(fabsf(configuracion_actual.setpoint - res.angulo) <= 8.0f)
                        res.flag_led = 1;
                        else res.flag_led = 0;
                        // Obtener hora desde RTC
                        SemaphoreHandle_t sem_time = xSemaphoreCreateBinary();
                        i2c_guard_t req_time_res = {
                            .dispositivo = I2C_RTC,
                            .data_out = &res.fecha,
                            .done = sem_time
                        };
                        xQueueSend(q_i2c_guard, &req_time_res, portMAX_DELAY);
                        xSemaphoreTake(sem_time, portMAX_DELAY);
                        vSemaphoreDelete(sem_time);
                        // Enviar a datalogger, guarda resultados
                        SemaphoreHandle_t sem_save = xSemaphoreCreateBinary();
                        datalogger_msg_t reqdlsave = {
                            .cmd = DATALOGGER_SAVE_RESULT,
                            .data = &res,
                            .done = sem_save
                        };
                        xQueueSend(q_datalogger, &reqdlsave, portMAX_DELAY);
                        xSemaphoreTake(sem_save, portMAX_DELAY); // esperar que terminen
                        vSemaphoreDelete(sem_save);
                        // Mensaje LCD
                        snprintf(msg.linea1, 16, "PID Desactivado");
                        snprintf(msg.linea2, 16, "Guarda Result. ");
                        xQueueSend(q_lcd, &msg, 0);
                        break;

                }
            } else if (estado_menu == 1) {
                if (tecla == '1' || tecla == '2') {
                    configuracion_actual.tipo_entrada = tecla == '2';
                    snprintf(msg.linea1, 16, "Setpoint:      ");
                    snprintf(msg.linea2, 16, "               ");
                    xQueueSend(q_lcd, &msg, 0);
                    estado_menu = (tecla == '1') ? 2 : 3;
                    valorset = 0;
                    digitsp = 0;

                }
            } else if (estado_menu == 2) {
                if (tecla >= '0' && tecla <= '9' && digitsp < 3) {
                    valorset = valorset * 10 + (tecla - '0');
                    digitsp++;
                    snprintf(msg.linea2, 16, "%3.0f            ", valorset);
                    xQueueSend(q_lcd, &msg, 0);
                } else if (tecla == '#') {
                    configuracion_actual.setpoint = fminf(valorset, 360.0f);
                    configuracion_actual.pendiente = 0;
                    snprintf(msg.linea1, 16, "Guardado       ");
                    snprintf(msg.linea2, 16, "               ");
                    xQueueSend(q_lcd, &msg, 0);
                    estado_menu = 0;
                } else if (tecla == '*') {
                    snprintf(msg.linea1, 16, "Cancelado      ");
                    snprintf(msg.linea2, 16, "               ");
                    xQueueSend(q_lcd, &msg, 0);
                    estado_menu = 0;
                }
            } else if (estado_menu == 3) {
                if (tecla >= '0' && tecla <= '9' && digitsp < 3) {
                    valorset = valorset * 10 + (tecla - '0');
                    digitsp++;
                    snprintf(msg.linea2, 16, "%3.0f            ", valorset);
                    xQueueSend(q_lcd, &msg, 0);
                } else if (tecla == '#') {
                    if (estado_menu == 3){
                    snprintf(msg.linea1, 16, "Pendiente:     ");
                    snprintf(msg.linea2, 16, "               ");
                    xQueueSend(q_lcd, &msg, 0);
                    valorpend = 0;
                    digitpend = 0;
                    estado_menu = 4;
                    }
                } else if (tecla == '*') {
                    snprintf(msg.linea1, 16, "Cancelado      ");
                    snprintf(msg.linea2, 16, "               ");
                    xQueueSend(q_lcd, &msg, 0);
                    estado_menu = 0;
                }
            } else if (estado_menu == 4) {
                    if (tecla >= '0' && tecla <= '9' && digitpend < 3) {
                    valorpend = valorpend * 10 + (tecla - '0');
                    digitpend++;
                    snprintf(msg.linea2, 16, "%3.0f            ", valorpend);
                    xQueueSend(q_lcd, &msg, 0);
                    } else if (tecla == '#') {
                        configuracion_actual.setpoint = fminf(valorset, 360.0f);
                        configuracion_actual.pendiente = fminf(valorpend, 100.0f);
                        snprintf(msg.linea1, 16, "Guardado       ");
                        snprintf(msg.linea2, 16, "               ");
                        xQueueSend(q_lcd, &msg, 0);
                        estado_menu = 0;
                    } else if (tecla == '*') {
                        if (configuracion_actual.pendiente == 0){
                        configuracion_actual.tipo_entrada = 0;
                        }
                        snprintf(msg.linea1, 16, "Cancelado      ");
                        snprintf(msg.linea2, 16, "               ");
                        xQueueSend(q_lcd, &msg, 0);
                        estado_menu = 0;
                }
            }
        }
}

// Task Init
void task_init(void *params) {
    i2c_init(I2C_PORT, 100000);
    gpio_set_function(SDA, GPIO_FUNC_I2C);
    gpio_set_function(SCL, GPIO_FUNC_I2C);
    gpio_pull_up(SDA); 
    gpio_pull_up(SCL);
    gpio_init(LED_VERDE); gpio_set_dir(LED_VERDE, GPIO_OUT); gpio_put(LED_VERDE, 1);
    gpio_init(LED_ROJO);  gpio_set_dir(LED_ROJO,  GPIO_OUT); gpio_put(LED_ROJO,  1);
    gpio_init(IN1);  gpio_set_dir(IN1,  GPIO_OUT); gpio_put(IN1,  0);
    gpio_init(IN2);  gpio_set_dir(IN2,  GPIO_OUT); gpio_put(IN2,  0);
    for (int i = 0; i < FIL; i++) {
        gpio_init(FILA_PINS[i]);
        gpio_set_dir(FILA_PINS[i], GPIO_OUT);
        gpio_put(FILA_PINS[i], 1);
    }
    for (int i = 0; i < COL; i++) {
        gpio_init(COL_PINS[i]);
        gpio_set_dir(COL_PINS[i], GPIO_IN);
        gpio_pull_up(COL_PINS[i]);
    }
    lcd_init(I2C_PORT, LCD_ADD);
    lcd_clear();
    lcd_string("Bienvenido");
    init_as5600();
    pwm_user_init(ENA, PWM_FRECUENCIA);

    q_lcd = xQueueCreate(2, sizeof(lcd_msg_t));
    q_angulo = xQueueCreate(1, sizeof(float));
    q_pid = xQueueCreate(1, sizeof(float));
    q_i2c_guard = xQueueCreate(8, sizeof(i2c_guard_t));
    q_datalogger = xQueueCreate(4, sizeof(datalogger_msg_t));
    q_pid_enable = xQueueCreate(1, sizeof(bool));

    vTaskDelete(NULL);
}

int main() {
    stdio_init_all();
    xTaskCreate(task_init,        "Init",        configMINIMAL_STACK_SIZE * 3, NULL, 4, NULL);
    xTaskCreate(task_lcd,         "LCD",         configMINIMAL_STACK_SIZE * 4, NULL, 1, NULL);
    xTaskCreate(task_keyboard,    "Keyboard",    configMINIMAL_STACK_SIZE * 4, NULL, 1, NULL);
    xTaskCreate(task_i2c_guard,   "I2C_Guard",   configMINIMAL_STACK_SIZE * 6, NULL, 2, NULL);
    xTaskCreate(task_encoder,     "Encoder",     configMINIMAL_STACK_SIZE * 2, NULL, 2, NULL);
    xTaskCreate(task_control_pid, "PID",         configMINIMAL_STACK_SIZE * 5, NULL, 3, NULL);
    xTaskCreate(task_flags,       "LEDS",        configMINIMAL_STACK_SIZE * 2, NULL, 2, NULL);
    xTaskCreate(task_datalogger,  "Datalogger",  configMINIMAL_STACK_SIZE * 4, NULL, 2, NULL);

    vTaskStartScheduler();
    while (1);
}


void set_motor_pwm(uint8_t gpio) {
    // Asigna función de PWM
    gpio_set_function(gpio, GPIO_FUNC_PWM);
    // Configura frecuencia de PWM e inicializa
    uint32_t slice = pwm_gpio_to_slice_num(gpio);
    pwm_set_clkdiv(slice, frequency_count_khz(CLOCKS_FC0_SRC_VALUE_CLK_SYS) / 1000.0);
    pwm_set_wrap(slice, 1000000 / PWM_FRECUENCIA);
    pwm_set_gpio_level(gpio, 500000 / PWM_FRECUENCIA);
    pwm_set_enabled(slice, true);
}

void motor_sentido_horario() {
    gpio_put(IN1, 1);
    gpio_put(IN2, 0);
}

void motor_sentido_antihorario() {
    gpio_put(IN1, 0);
    gpio_put(IN2, 1);
}
#ifndef __FIRMWARE_CONFIG_H__
#define __FIRMWARE_CONFIG_H__

#include <stdio.h>
#include <string.h>
#include "hardware/i2c.h"

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include "lcd.h"
#include "ina219.h"
#include "ds3231.h"
#include "sd_card.h"

// I2C definiciones
#define I2C_PORT i2c1
#define I2C_SDA 18
#define I2C_SCL 19
#define LCD_ADDR 0x27

#define MINIMUM_RESISTANCE 20
#define MAXIMUM_RESISTANCE 2000 // Resistencia máxima en Ohm

// Botones y Encoder
#define DEBOUNCE_TIME 50 // Tiempo de debounce en ms
#define BTN_MENU_GPIO 6
#define BTN_STOP_GPIO 7
#define BTN_SWITCH_GPIO 14
#define MAX_MENU_NUM 2

#define ENC_CHA_GPIO 12
#define ENC_CHB_GPIO 13
#define ENC_MAX_INDEX 2

// Controlador PID y protecciones
#define PWM_GAIN 1.372f
#define MAX_PWM_WRAP 12000

#define MAX_PWM_VOUT (float) (3.7f / PWM_GAIN)
#define MIN_PWM_VOUT (float) (3.02f / PWM_GAIN)

#define MAX_PWM_DUTY (uint16_t) (MAX_PWM_WRAP * MAX_PWM_VOUT / 3.3f)
#define MIN_PWM_DUTY (uint16_t) (MAX_PWM_WRAP * MIN_PWM_VOUT / 3.3f)
#define PWM_OFF (MIN_PWM_DUTY - (MAX_PWM_WRAP / 500))

#define PID_STATUS_PIN 15
#define PWM_PIN 16
#define ADC_DIODE_TEMP 0 // Pin 26
#define MAX_TEMP 130.0f
#define INA219_MAX_CURRENT 0.38f
#define MAX_CURRENT 0.270f
#define MAX_VOLTAGE 12.0f

#define Kp 8.2f
#define Kd 0.032f
#define Ki 0.035f
#define MAX_INTEGRAL_VALUE 5.0f
#define MAX_DERIVATIVE_VALUE 10.0f

#define CONTROLLER_REFRESH_MS 40
#define SLEEP_INA219    30
#define SLEEP_TIME_LCD  350 // Tiempo de espera en ms para la LCD

#define USE_SERIAL_LOGGER 0
#define LOGGER_CHUNK_SIZE 100
#define LOGGER_MIN_SEND 10

typedef struct {
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
} rtc_data_t;

typedef enum {
    BTN_MENU,
    BTN_STOP,
    BTN_SWITCH,
    ENCODER
} btn_devices_enum;

typedef struct  {
    uint8_t gpio;
    SemaphoreHandle_t *sem_bin;
    btn_devices_enum device;
} btn_data_t;

typedef struct encoder_t {
    bool cha;
    bool chb;
} encoder_t;

typedef enum {
    MENU_MAIN = 255,
    MENU_SET_RESISTANCE = 0,
    MENU_PID = 1,
    MENU_TEST = 2,
    MENU_TIME = 3,
    MENU_SD = 4,
    // MENU_REG_FUENTE = 5,
    MENU_PROTECCION = 6
} menu_t;

typedef enum  {
    I2C_INA219,
    I2C_LCD,
    I2C_RTC
} i2c_devices_enum;

typedef struct {
    i2c_devices_enum device;
    QueueHandle_t queue;
    void (*callback)(void * param);
    void * param;
} i2c_guard_t;

typedef struct {
    btn_devices_enum device;
    bool increment;
} input_data_t;

typedef struct {
    menu_t menu;
    uint8_t index;
    bool fixed_index;
    bool sd_mounted;
    uint8_t sd_file_count;

    bool pid_enabled;
    uint16_t pid_time_ms;
    uint16_t pwm_value;
  
    uint16_t resistance_target;
    int16_t resistance_adj;
} system_config_t;

typedef struct {
    float kp;
    float ki;
    float kd;
    uint16_t r_target;
    uint16_t pid_time;
} pid_config_t;

typedef struct {
    float voltage_v;
    float current_ma;
    uint16_t pwm_value;
    float error;
    float integral;
    float derivative;
    uint16_t r_target;
    float temperature;
} datalogger_t;

typedef struct {
    uint16_t pid_time_ms;
    uint16_t r_target;
    uint8_t r_step_idx;
} config_t;

typedef enum {
    CONFIG_FILE,
    LOG_FILE
} sd_input_t;

typedef struct {
    sd_input_t type;
    void *data;
    uint8_t chunk_index;
} sd_event_t;

const char file_header[] = "Voltage;Current;PWM Value;Error;Integral;Derivative;R_Target;Temperature\n";
const uint8_t r_steps[] = {1, 10, 50, 100, 250};

void btn_irq_handler(uint gpio, uint32_t events);
void task_encoder(void *pvParameters);
void task_btn_pull_up(void *pvParameters);
void task_pid_controller(void *pvParameters);
void task_i2c_guard(void *pvParameters);
void task_ina219(void *pvParameters);
void task_lcd_display(void *pvParameters);
void task_read_temp(void *pvParameters);
void task_rtc(void *pvParameters);

void setup_pwm(uint8_t gpio);
void set_lcd_text(void *text);
void limit_float(float *value, float max);
void wrap_index(uint8_t *index, uint8_t max_index, bool increment);

#endif // __FIRMWARE_CONFIG_H__
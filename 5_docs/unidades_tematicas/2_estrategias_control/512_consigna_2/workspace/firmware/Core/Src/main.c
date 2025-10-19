/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2025 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under BSD 3-Clause license,
  * the "License"; You may not use this file except in compliance with the
  * License. You may obtain a copy of the License at:
  *                        opensource.org/licenses/BSD-3-Clause
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "lcd_i2c.h"
#include <stdio.h>
#include <string.h>
#include "semphr.h"
#include "math.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

#define MIN_PWM       200    // valor mínimo de PWM para mover el motor

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
I2C_HandleTypeDef hi2c2;

TIM_HandleTypeDef htim1;

UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */

QueueHandle_t I2C_txQueue;
QueueHandle_t I2C_rxQueue;
QueueHandle_t angleQueue;
QueueHandle_t rtcQueue;
QueueHandle_t angleQueuePID;
QueueHandle_t controlQueue;

SemaphoreHandle_t sem_btn0;
SemaphoreHandle_t sem_btn1;
SemaphoreHandle_t sem_btn2;
SemaphoreHandle_t sem_btn3;

typedef struct {
    float angle;   // setpoint en grados
    uint8_t mode;     // 0 = lento, 1 = rápido
    float margen;  // margen del setpoint
} ControlParams_t;

typedef struct {
    uint16_t angle;   // 0–360
    uint8_t mode;     // Control mode: 0=normal, 1=lento
} setpoint_t;


volatile uint8_t editing_setpoint = 0;
volatile uint8_t editing_datalogg = 0;
volatile uint8_t edit_sp_field = 0; // 0 = centenas, 1 = decenas, 2 = unidades, 3 = modo
volatile uint8_t last_index=99;
volatile uint16_t datalog_index = 0;   // índice actual (0..datalog_count-1)
volatile uint16_t datalog_count = 15;   // cantidad de líneas guardadas (get_data_offset_line)


uint8_t datalogg = 0;

uint8_t editing_margin = 0;
int edit_margin_field = 0;   // 0 = decenas, 1 = unidades


int margin_value = 1;                      // Margen de 1 Grado por defecto en Tarea button
volatile setpoint_t sp_editing = {0, 1};   // Setpoint en 0 grados y Modo lento en Tarea button

typedef enum {
    RECEIVE,
    TRANSMIT
} i2c_op_t;

typedef struct {
    uint16_t device_id;
    uint8_t reg;
    i2c_op_t type;
    uint8_t n_bytes;
    uint8_t *data;
} i2cQueue_t;

typedef struct {
    uint8_t sec, min, hour, day, month, year;
} rtc_time_t;

typedef enum {
    MENU_SETPOINT = 0,
    MENU_SET_TIME,
	MENU_MARGEN,
	MENU_DATALOGGER,
	MENU_TOTAL_ITEMS
} menu_option_t;

typedef struct {
    uint16_t sec_min, hour_day, month_year;
} rtc_time_halfword_t;

volatile menu_option_t menu_selected = MENU_SETPOINT;
volatile uint8_t editing_time = 0;
volatile uint8_t edit_field = 0;
rtc_time_t rtc_editing;

// PID variables

float kp = 5.0f;
float ki = 0.002f;
float kd = 0.1f;
float pid_integral = 0.0f;
float pid_last_error = 0.0f;

// PWM limits
uint16_t pwm_max = 500;   // Máxima salida
uint16_t pwm_slow = 250;  // Modo lento

uint32_t flashAddress = 0x0800FC00; // Last 1KB page (page 63)
#define FLASH_ADDRESS_PAGE 63

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_I2C2_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_TIM1_Init(void);
/* USER CODE BEGIN PFP */

void write_flash(uint32_t address, uint32_t offset, uint16_t data);
void read_flash(uint8_t* readValue,uint8_t offset);
void erase_flash(uint32_t pageAddress);
int  save_struct_flash(const rtc_time_t *data, float error, float setpoint); //1 si error , 0 si ok
void save_time_data(const rtc_time_t *data, uint32_t offset);
void save_data_value(float setpoint, float error, uint32_t offset);
uint8_t get_data_offset();
void read_time_date(rtc_time_t *data,uint32_t line);
void read_saved_sp(float *data,uint32_t line);
void read_saved_err(float *data,uint32_t line);
uint32_t get_data_offset_line();

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

void write_flash(uint32_t address, uint32_t offset, uint16_t data){
	//uint32_t flashAddress = 0x0800FC00; // Last 1KB page (page 63)
    if(address < 0x08000000 || address >= 0x08010000) {
        return; // Invalid address
    }
    //SOLO PUEDO ESCRIBIR DE A MEDIA PALABRA = 16BITS
    HAL_FLASH_Unlock();
    uint32_t write_address = address+(1*offset);
    HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD, write_address, data);
    HAL_FLASH_Lock();
}
void read_flash(uint8_t* readValue,uint8_t offset){

    *readValue = *(__IO uint8_t*)(flashAddress+(offset));
}
void read_time_date(rtc_time_t *data,uint32_t line){
	rtc_time_t temp;
	read_flash(&temp.sec, 	0x10*line +	0);
	read_flash(&temp.min, 	0x10*line +	1);
	read_flash(&temp.hour, 	0x10*line +	2);
	read_flash(&temp.day, 	0x10*line +	3);
	read_flash(&temp.month, 	0x10*line +	4);
	read_flash(&temp.year, 	0x10*line +	5);

	*data=temp;
}
void read_saved_sp(float *data,uint32_t line){
	uint8_t temp_int;
	uint8_t temp_flt;
	read_flash(&temp_int, 	0x10*line +	8);
	read_flash(&temp_flt, 	0x10*line +	9);
	*data	=	(float)temp_int + ((float)temp_flt / 100.0f);
}
void read_saved_err(float *data,uint32_t line){
	uint8_t temp_int;
	uint8_t temp_flt;
	read_flash(&temp_int, 	0x10*line +	12);
	read_flash(&temp_flt, 	0x10*line +	13);
	*data	=	(float)temp_int + ((float)temp_flt / 100.0f);
}

void erase_flash(uint32_t pageAddress){
	//HACER FULL ERASE LA PRIMERA VEZ QUE CORRE EL PROGRAMA
	    if((pageAddress < 0x08000000) ||
	       (pageAddress >= 0x08010000) ||
	       ((pageAddress & 0x3FF) != 0)) { // Checkeo 1KB alineamiento
	        return;
	    }
	    FLASH_EraseInitTypeDef eraseInit;
	    eraseInit.TypeErase = FLASH_TYPEERASE_PAGES;
	    eraseInit.PageAddress = pageAddress;
	    eraseInit.NbPages = 1;
	    uint32_t PageError;
	    HAL_FLASH_Unlock();
	    HAL_FLASHEx_Erase(&eraseInit, &PageError);
	    HAL_FLASH_Lock();
}
int save_struct_flash(const rtc_time_t *data, float error, float setpoint){
	uint8_t offset=get_data_offset();
	if(offset>=250) return 1;	//maximo 15 valores a guardar
	save_time_data(data, offset);
	save_data_value(setpoint, error, offset);
	return 0;
}

void save_time_data(const rtc_time_t *data, uint32_t offset){
	//en bytes=
	//SEG,MIN ; HORA,DIA ; MES,ANIO ; 0XAA,0XAA ; SP(INT),SP(FLOAT) ; 0XAA,0XAA ; ERR(INT),ERR(FLOAT)
	//escribo en halfwords
	rtc_time_halfword_t half_word_data;
	memcpy(&half_word_data, data, sizeof(rtc_time_t));
	write_flash(flashAddress,offset, half_word_data.sec_min);
	write_flash(flashAddress,2+offset, half_word_data.hour_day);
	write_flash(flashAddress,4+offset, half_word_data.month_year);
	write_flash(flashAddress,6+offset, 0xAAAA);
}
void save_data_value(float setpoint, float error, uint32_t offset){
	uint32_t off=offset+8;
	uint8_t sp_int,err_int;
	uint8_t sp_flt,err_flt;
	uint16_t sp,err;
	sp_int=(int)setpoint;
	err_int=(int)error;
	sp_flt=(int)((setpoint-sp_int)*100);
	err_flt=(int)((error-err_int)*100);
	sp=sp_flt<<8 | sp_int;
	err=err_flt<<8 | err_int;

	write_flash(flashAddress, off, sp);
	write_flash(flashAddress,2+off, 0xAAAA);
	write_flash(flashAddress,4+off, err);
	write_flash(flashAddress,6+off, 0xAAAA);
}
uint8_t get_data_offset(){
	//SIEMPRE CORRER ESTO PRIMERO PARA OBTENER EL OFFSET
	int temp=0;
	uint8_t read_value;
	uint8_t last_read_value=0x00;
	uint8_t last_last_read_value=0xFF;
	for(temp=0;temp<200;temp++){
		read_flash(&read_value,temp);
		if(read_value==0xFF && last_read_value==0xAA && last_last_read_value==0xAA) {
			return (temp);
		}
		if(read_value==0xFF && last_read_value==0xFF && last_last_read_value==0xFF) {
			return (0);
		}
		last_last_read_value=last_read_value;
		last_read_value=read_value;
	}
	return 0;
	//La func retorna el offset de flashadress donde escribir el prox valor
}
uint32_t get_data_offset_line(){
	uint8_t temp=0;
	temp=get_data_offset();
	if(temp==0)return 0;
	else return (temp>>4);
}

static float angle_error(float set, float current) {
    float e = set - current;
    if (e > 180.0f)  e -= 360.0f;
    if (e < -180.0f) e += 360.0f;
    return e;
}

static void motor_stop(void) {
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_2, 0);
    HAL_GPIO_WritePin(IN1_GPIO_Port, IN1_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(IN2_GPIO_Port, IN2_Pin, GPIO_PIN_RESET);
}

void I2C_controllerTask(void *pvParameters) {
    i2cQueue_t i2cq;
    uint8_t temp[8];
    while (1) {
        xQueueReceive(I2C_txQueue, &i2cq, portMAX_DELAY);
        if (i2cq.type == RECEIVE) {
            HAL_I2C_Master_Transmit(&hi2c2, i2cq.device_id, &i2cq.reg, 1, 10);
            HAL_I2C_Master_Receive(&hi2c2, i2cq.device_id, temp, i2cq.n_bytes, 10);
            xQueueSend(I2C_rxQueue, temp, portMAX_DELAY);
        } else {
            temp[0] = i2cq.reg;
            for (uint8_t i = 0; i < i2cq.n_bytes; i++) temp[i + 1] = i2cq.data[i];
            HAL_I2C_Master_Transmit(&hi2c2, i2cq.device_id, temp, i2cq.n_bytes + 1, 10);
        }
    }
}
void as5600_readerTask(void *pvParameters) {
    uint8_t raw_data[2];
    float angle_deg;

    i2cQueue_t i2cq;
    i2cq.device_id = 0x36 << 1;
    i2cq.reg = 0x0E;  // ANGLE register (MSB)
    i2cq.type = RECEIVE;
    i2cq.n_bytes = 2;
    i2cq.data = raw_data;

    while (1) {
        xQueueSend(I2C_txQueue, &i2cq, portMAX_DELAY);
        xQueueReceive(I2C_rxQueue, raw_data, portMAX_DELAY);

        uint16_t raw_angle = ((uint16_t)raw_data[0] << 8) | raw_data[1];
        angle_deg = (float)raw_angle * 360.0f / 4096.0f;

        xQueueOverwrite(angleQueuePID, &angle_deg);
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

void pid_controlTask(void *pvParameters) {
    float angle;
    float last_error = 0.0f;
    ControlParams_t params;   // Para recibir de controlQueue

    // Control mode: 0=normal, 1=lento

    uint8_t slow_mode = 1;      // modo lento en el Encendido
    float DEADBAND_DEG = 1.0f;  // Margen en 1 grado en el Encendido
    float setpoint = 0.0f - 26.0f;    // Setpoint en 0 grados en el Encendido


    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_2);
    TickType_t last_tick = xTaskGetTickCount();

    for (;;) {
        xQueueReceive(angleQueuePID, &angle, portMAX_DELAY);

        if (xQueueReceive(controlQueue, &params, 0) == pdTRUE) {
               setpoint   = params.angle - 26.0f;          // actualizar setpoint
               slow_mode  = params.mode;           // actualizar modo
               DEADBAND_DEG = params.margen;       // actualizar margen
               }

        // Calcular dt real
        TickType_t now_tick = xTaskGetTickCount();
        float dt = (now_tick - last_tick) / (float)configTICK_RATE_HZ;
        if (dt <= 0) dt = 0.001f;
        last_tick = now_tick;

        float error = angle_error(setpoint, angle);

        // Si está dentro de la zona muerta, detener motor
        if (fabs(error) <= DEADBAND_DEG) {
            motor_stop();
            HAL_GPIO_WritePin(LED_RANGO_GPIO_Port, LED_RANGO_Pin, GPIO_PIN_RESET);
            pid_integral = 0.0f;
            vTaskDelay(pdMS_TO_TICKS(20));
            continue; // se utiliza para salir del loop de la tarea si se cumple la condicion fabs(error) <= DEADBAND_DEG
        }else{
        	HAL_GPIO_WritePin(LED_RANGO_GPIO_Port, LED_RANGO_Pin, GPIO_PIN_SET);
        }

        // PID
        pid_integral += error * dt;
        float derivative = (error - last_error) / dt;
        last_error = error;

        float output = kp * error + ki * pid_integral + kd * derivative;

        // Limitar salida
        float limit = slow_mode ? pwm_slow : pwm_max;
        if (output > limit) output = limit;
        if (output < -limit) output = -limit;

        // Dirección
        if (output > 0) {
            HAL_GPIO_WritePin(IN1_GPIO_Port, IN1_Pin, GPIO_PIN_SET);
            HAL_GPIO_WritePin(IN2_GPIO_Port, IN2_Pin, GPIO_PIN_RESET);
        } else {
            HAL_GPIO_WritePin(IN1_GPIO_Port, IN1_Pin, GPIO_PIN_RESET);
            HAL_GPIO_WritePin(IN2_GPIO_Port, IN2_Pin, GPIO_PIN_SET);
        }
        // Forzar PWM mínimo cuando no está en la zona muerta
        if (fabs(output) < MIN_PWM) {
            if (output > 0) output = MIN_PWM;
            else output = -MIN_PWM;
        }

        // PWM
        __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_2, fabs(output));

        vTaskDelay(pdMS_TO_TICKS(20)); // ~50 Hz loop
    }
}

void buttonHandlerTask(void *pvParameters) {
    TickType_t lastPress[4] = {0};

    while (1) {
        TickType_t now = xTaskGetTickCount();

        // --- BOTÓN 1 (PA0): cambiar opción o guardar valores ---
        if (xSemaphoreTake(sem_btn0, 0) == pdTRUE && (now - lastPress[0] > pdMS_TO_TICKS(200))) {
            if (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_3) == GPIO_PIN_RESET) {
                lastPress[0] = now;

                if (!editing_time && !editing_setpoint && !editing_margin) {
                    menu_selected = (menu_selected + 1) % MENU_TOTAL_ITEMS;
                } else if (editing_time) {
                    // Guardar RTC en DS1307
                    uint8_t buf[7];
                    buf[0] = (((rtc_editing.sec / 10) << 4) | (rtc_editing.sec % 10)) & 0x7F;
                    buf[1] = ((rtc_editing.min/10)<<4)|(rtc_editing.min%10);
                    buf[2] = ((rtc_editing.hour/10)<<4)|(rtc_editing.hour%10);
                    buf[3] = 1;
                    buf[4] = ((rtc_editing.day/10)<<4)|(rtc_editing.day%10);
                    buf[5] = ((rtc_editing.month/10)<<4)|(rtc_editing.month%10);
                    buf[6] = ((rtc_editing.year/10)<<4)|(rtc_editing.year%10);
                    i2cQueue_t i2cq = {0x68 << 1, 0x00, TRANSMIT, 7, buf};
              //      xQueueSend(I2C_txQueue, &i2cq, portMAX_DELAY);
                    editing_time = 0;
                }else if (editing_setpoint) {
                    // Guardar setpoint definitivo
                    editing_setpoint = 0;
                    rtc_time_t placeholder={99,99,99,22,22,22};
                    save_struct_flash(&placeholder, sp_editing.angle, margin_value);
                }else if (editing_margin) {
                    // Guardar margen definitivo
                    editing_margin = 0;
                }
                if (editing_datalogg) {         // salir de DATALOGGER
                    editing_datalogg = 2;
                    last_index = 99;
                }

                ControlParams_t msg;
                msg.angle  = sp_editing.angle;
                msg.mode   = sp_editing.mode;
                msg.margen = margin_value;
                xQueueOverwrite(controlQueue, &msg);
            }
        }

        // --- BOTÓN 2 (PA1): entrar o cambiar campo ---
        if (xSemaphoreTake(sem_btn1, 0) == pdTRUE && (now - lastPress[1] > pdMS_TO_TICKS(200))) {
            if (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_15) == GPIO_PIN_RESET) {
                lastPress[1] = now;
                if(menu_selected == MENU_DATALOGGER && editing_datalogg && !datalogg){
                	erase_flash(flashAddress);
                	datalog_index = 0;
                	editing_datalogg = 2;
                	last_index = 99;
                }
                if (menu_selected == MENU_DATALOGGER && !datalogg){
                					datalog_index = 0;
                					last_index = 99;
                                	editing_datalogg = 1;
                                }
                else if (menu_selected == MENU_SET_TIME && !editing_time) {
                       if (xQueuePeek(rtcQueue, &rtc_editing, 0) != pdTRUE) {
                        rtc_editing.hour = 0;
                        rtc_editing.min = 0;
                        rtc_editing.sec = 0;
                        rtc_editing.day = 1;
                        rtc_editing.month = 1;
                        rtc_editing.year = 0;
                    }
                    editing_time = 1;
                    edit_field = 0; // empezamos siempre en hora
                    // Forzar un "redibujado" inmediato del menú

                } else if (menu_selected == MENU_SETPOINT && !editing_setpoint) {
                    editing_setpoint = 1;
                    edit_sp_field = 0; // empezar en centenas
                } else if (menu_selected == MENU_MARGEN && !editing_margin) {
                     editing_margin = 1;
                     edit_margin_field = 0; // empezar en decenas

                      } else if (editing_time) {
                         edit_field = (edit_field + 1) % 6;

                      } else if (editing_setpoint) {
                          edit_sp_field = (edit_sp_field + 1) % 4;

                      } else if (editing_margin) {
                          edit_margin_field = (edit_margin_field + 1) % 2; // decenas o unidades
                      }
            }
        }

        // --- BOTÓN 3 (PA2): subir valor ---
        if (xSemaphoreTake(sem_btn2, 0) == pdTRUE && (now - lastPress[2] > pdMS_TO_TICKS(200))) {
            if (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_5) == GPIO_PIN_RESET) {
                lastPress[2] = now;

                if (editing_time) {
                    switch (edit_field) {
                        case 0: if (++rtc_editing.hour > 23) rtc_editing.hour = 0; break;
                        case 1: if (++rtc_editing.min > 59)  rtc_editing.min = 0; break;
                        case 2: if (++rtc_editing.sec > 59)  rtc_editing.sec = 0; break;
                        case 3: if (++rtc_editing.day > 31)  rtc_editing.day = 1; break;
                        case 4: if (++rtc_editing.month > 12)rtc_editing.month = 1; break;
                        case 5: if (++rtc_editing.year > 99) rtc_editing.year = 0; break;
                    }
                }else if (editing_setpoint) {
                	     if (edit_sp_field == 0) { // centenas
                	            if (sp_editing.angle + 100 <= 360) sp_editing.angle += 100;
                	        } else if (edit_sp_field == 1) { // decenas
                	            if (sp_editing.angle + 10 <= 360) sp_editing.angle += 10;
                	        } else if (edit_sp_field == 2) { // unidades
                	            if (sp_editing.angle + 1 <= 360) sp_editing.angle += 1;
                	        } else if (edit_sp_field == 3) { // modo rápido/lento
                	            sp_editing.mode = !sp_editing.mode;
                	        }
                } else if (editing_margin) {
                                   if (edit_margin_field == 0) { // decenas
                                       if (margin_value + 10 <= 45) margin_value += 10;
                                   } else if (edit_margin_field == 1) { // unidades
                                       if (margin_value + 1 <= 45) margin_value += 1;
                                   }
                }else if (editing_datalogg) {
                    datalog_count = get_data_offset_line();
                    if (datalog_count > 0 && datalog_index + 1 < datalog_count) {
                        datalog_index++;                              // siguiente registro
                    }
                }
            }
        }

        // --- BOTÓN 4 (PA3): bajar valor ---
        if (xSemaphoreTake(sem_btn3, 0) == pdTRUE && (now - lastPress[3] > pdMS_TO_TICKS(200))) {
            if (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_4) == GPIO_PIN_RESET) {
                lastPress[3] = now;

                if (editing_time) {
                    switch (edit_field) {
                        case 0: rtc_editing.hour  = (rtc_editing.hour == 0) ? 23 : rtc_editing.hour - 1; break;
                        case 1: rtc_editing.min   = (rtc_editing.min == 0)  ? 59 : rtc_editing.min - 1; break;
                        case 2: rtc_editing.sec   = (rtc_editing.sec == 0)  ? 59 : rtc_editing.sec - 1; break;
                        case 3: rtc_editing.day   = (rtc_editing.day == 1)  ? 31 : rtc_editing.day - 1; break;
                        case 4: rtc_editing.month = (rtc_editing.month == 1)? 12 : rtc_editing.month - 1; break;
                        case 5: rtc_editing.year  = (rtc_editing.year == 0) ? 99 : rtc_editing.year - 1; break;
                    }
                }else if (editing_setpoint) {
                         if (edit_sp_field == 0) { // centenas
                            if (sp_editing.angle >= 100) sp_editing.angle -= 100;
                    } else if (edit_sp_field == 1) { // decenas
                        if (sp_editing.angle >= 10) sp_editing.angle -= 10;
                    } else if (edit_sp_field == 2) { // unidades
                        if (sp_editing.angle >= 1) sp_editing.angle -= 1;
                    } else if (edit_sp_field == 3) { // modo rápido/lento
                        sp_editing.mode = !sp_editing.mode;
                    }
                }
                else if (editing_margin) {
                                    if (edit_margin_field == 0) { // decenas
                                        if (margin_value >= 10) margin_value -= 10;
                                    } else if (edit_margin_field == 1) { // unidades
                                        if (margin_value > 1) margin_value -= 1;
                                    }
                }else if (editing_datalogg) {
                    if (datalog_index > 0) {
                        datalog_index--;                              // registro anterior
                    }
                }

            }
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}



void menuTask(void *pvParameters) {
    lcd_init();
    lcd_clear();
    char buffer[17];
    TickType_t last_blink = 0;
    uint8_t blink_state = 1;
    uint8_t last_editing_time = 0;
    uint8_t last_editing_setpoint = 0;
    uint8_t last_editing_margin = 0;
    uint8_t last_editing_datalogg = 2;
    rtc_time_t date_time_print = {0,0,0,0,0,0};
    uint8_t linea_data = 0;
    //uint8_t last_index=99;


    float 	sp_print,err_print = 0.0f;

    while (1) {
        TickType_t now = xTaskGetTickCount();
        if (now - last_blink >= pdMS_TO_TICKS(500)) {
            blink_state = !blink_state;
            last_blink = now;
        }

        if (!editing_time && !editing_setpoint && !editing_margin && !editing_datalogg ) {
            // --- Menú principal ---
            lcd_goto_XY(0, 0);
            lcd_send_string(menu_selected == MENU_SETPOINT ? "> Setpoint " : "  Setpoint ");
            lcd_goto_XY(1, 0);
            lcd_send_string(menu_selected == MENU_SET_TIME ? "> Set time/date" : "  Set time/date");
            lcd_goto_XY(2, 0);
            lcd_send_string(menu_selected == MENU_MARGEN ? "> Margen Setpoint " : "  Margen Setpoint ");
            lcd_goto_XY(3, 0);
            lcd_send_string(menu_selected == MENU_DATALOGGER ? "> Datalogger " : "  Datalogger ");
        }
        else if (editing_time) {
            // --- Edición de hora/fecha ---
            // Línea 0: Hora
            lcd_goto_XY(0, 0);
            if (edit_field == 0 && !blink_state) lcd_send_string("  ");
            else { snprintf(buffer, sizeof(buffer), "%02u", rtc_editing.hour); lcd_send_string(buffer); }
            lcd_send_string(":");
            if (edit_field == 1 && !blink_state) lcd_send_string("  ");
            else { snprintf(buffer, sizeof(buffer), "%02u", rtc_editing.min); lcd_send_string(buffer); }
            lcd_send_string(":");
            if (edit_field == 2 && !blink_state) lcd_send_string("  ");
            else { snprintf(buffer, sizeof(buffer), "%02u", rtc_editing.sec); lcd_send_string(buffer); }

            // Línea 1: Fecha
            lcd_goto_XY(1, 0);
            if (edit_field == 3 && !blink_state) lcd_send_string("  ");
            else { snprintf(buffer, sizeof(buffer), "%02u", rtc_editing.day); lcd_send_string(buffer); }
            lcd_send_string("/");
            if (edit_field == 4 && !blink_state) lcd_send_string("  ");
            else { snprintf(buffer, sizeof(buffer), "%02u", rtc_editing.month); lcd_send_string(buffer); }
            lcd_send_string("/");
            if (edit_field == 5 && !blink_state) lcd_send_string("  ");
            else { snprintf(buffer, sizeof(buffer), "%02u", rtc_editing.year); lcd_send_string(buffer); }
        }
        else if (editing_setpoint) {
            // --- Edición de setpoint ---
            lcd_goto_XY(0, 0);
            lcd_send_string("Setpoint:");

            // Línea 1: ángulo con centenas/decenas/unidades
            lcd_goto_XY(1, 0);
            snprintf(buffer, sizeof(buffer), "%03u %c   ", sp_editing.angle,0xDF);

            if (edit_sp_field == 0 && !blink_state) buffer[0] = ' ';
            if (edit_sp_field == 1 && !blink_state) buffer[1] = ' ';
            if (edit_sp_field == 2 && !blink_state) buffer[2] = ' ';
            lcd_send_string(buffer);

            // Línea 2: modo rápido/lento
            lcd_goto_XY(2, 0);
            if (edit_sp_field == 3 && !blink_state) {
                lcd_send_string("              ");
            } else {
                lcd_send_string(sp_editing.mode ? "Modo: Lento" : "Modo: Rapido");
            }
        }

        else if (editing_margin) {
                    // --- Edición de margen ---
                    lcd_goto_XY(0, 0);
                    lcd_send_string("Margen SP:");

                    // Línea 1: valor del margen con decenas/unidades
                    lcd_goto_XY(1, 0);
                    snprintf(buffer, sizeof(buffer), "%02u%c     ", margin_value, 0xDF);

                    if (edit_margin_field == 0 && !blink_state) buffer[0] = ' ';
                    if (edit_margin_field == 1 && !blink_state) buffer[1] = ' ';
                    lcd_send_string(buffer);
                }
                    else if (editing_datalogg){
                	// --- Impresión de datalog ---
                    	linea_data = get_data_offset_line();
                    	        	lcd_goto_XY(0, 0);
                    	        	//cursor : datalog_index
                    	        	if(linea_data==0 && last_index != datalog_index){
                    	        		lcd_clear();
                    	        		lcd_send_string("No data");
                    	        		last_index = datalog_index;
                    	        	}
                    	        	if(linea_data>=1 && last_index != datalog_index) {
                    	        		lcd_clear();
                    	        		read_saved_sp(&sp_print, datalog_index);
                    	        		read_saved_err(&err_print, datalog_index);
                    	        		read_time_date(&date_time_print, datalog_index);

                    	        		lcd_send_string("Log n");
                    	        		        		snprintf(buffer, sizeof(buffer), "%d", datalog_index+1);
                    	        		        		lcd_send_string(buffer);
                    	        		        		lcd_send_string(": ");
                    	        		        		snprintf(buffer, sizeof(buffer), "%02u", date_time_print.sec);
                    	        		        		lcd_send_string(buffer);
                    	        		        		lcd_send_string("/");
                    	        		        		snprintf(buffer, sizeof(buffer), "%02u", date_time_print.min);
                    	        		        		lcd_send_string(buffer);
                    	        		        		lcd_send_string("/");
                    	        		        		snprintf(buffer, sizeof(buffer), "%02u", date_time_print.hour);
                    	        		        		lcd_send_string(buffer);

                    	        		        		lcd_goto_XY(1, 0);
                    	        		        		lcd_send_string("         ");
                    	        		        		snprintf(buffer, sizeof(buffer), "%02u", date_time_print.day);
                    	        		        		lcd_send_string(buffer);
                    	        		        		lcd_send_string("/");
                    	        		        		snprintf(buffer, sizeof(buffer), "%02u", date_time_print.month);
                    	        		        		lcd_send_string(buffer);
                    	        		        		lcd_send_string("/");
                    	        		        		snprintf(buffer, sizeof(buffer), "%02u", date_time_print.year);
                    	        		        		lcd_send_string(buffer);

                    	        		        		lcd_goto_XY(2, 0);
                    	        		        		lcd_send_string("E: ");
                    	        		        		snprintf(buffer, sizeof(buffer), "%.2f", sp_print);
                    	        		        		lcd_send_string(buffer);

                    	        		        		lcd_send_string(" S: ");
                    	        		        		snprintf(buffer, sizeof(buffer), "%.2f", err_print);
                    	        		        		lcd_send_string(buffer);

                    	        		        		lcd_goto_XY(3, 0);
                    	        		        		lcd_send_string("Datos totales = ");
                    	        		        		snprintf(buffer, sizeof(buffer), "%02u", linea_data);
                    	        		        		lcd_send_string(buffer);
                    	        		        		last_index = datalog_index;
                    	        	}
                 }

        // Limpieza de pantalla al entrar/salir de edición
        if ((editing_time != last_editing_time) || (editing_setpoint != last_editing_setpoint)|| (editing_margin != last_editing_margin) || (editing_datalogg == last_editing_datalogg)) {
            for (int i = 0; i < 4; i++) {
                lcd_goto_XY(i, 0);
                lcd_send_string("                    "); // borra toda la línea
            }
            editing_datalogg = 0;
            last_editing_time = editing_time;
            last_editing_setpoint = editing_setpoint;
            last_editing_margin = editing_margin;
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

/*
void rtcSerialDebugTask(void *pvParameters) {
    i2cQueue_t i2cq;
    uint8_t data[7];
    rtc_time_t rtc_now;
    char buffer[64];

    while (1) {
        // Pedir lectura al DS1307 (7 bytes desde reg 0x00)
        i2cq.device_id = 0x68 << 1;
        i2cq.reg = 0x00;
        i2cq.type = RECEIVE;
        i2cq.n_bytes = 7;
        i2cq.data = NULL;

        xQueueSend(I2C_txQueue, &i2cq, portMAX_DELAY);

        if (xQueueReceive(I2C_rxQueue, data, pdMS_TO_TICKS(50)) == pdTRUE) {
            // Convertir de BCD a binario
            rtc_now.sec   = ((data[0] >> 4) * 10) + (data[0] & 0x0F);
            rtc_now.min   = ((data[1] >> 4) * 10) + (data[1] & 0x0F);
            rtc_now.hour  = ((data[2] >> 4) * 10) + (data[2] & 0x0F);
            rtc_now.day   = ((data[4] >> 4) * 10) + (data[4] & 0x0F);
            rtc_now.month = ((data[5] >> 4) * 10) + (data[5] & 0x0F);
            rtc_now.year  = ((data[6] >> 4) * 10) + (data[6] & 0x0F);

            // Mostrar por UART
            snprintf(buffer, sizeof(buffer), "Fecha/Hora: %02u/%02u/%02u %02u:%02u:%02u\r\n",
                     rtc_now.day, rtc_now.month, rtc_now.year,
                     rtc_now.hour, rtc_now.min, rtc_now.sec);
            HAL_UART_Transmit(&huart2, (uint8_t*)buffer, strlen(buffer), HAL_MAX_DELAY);
        }

        vTaskDelay(pdMS_TO_TICKS(2000)); // Esperar 2 segundos
    }
}

*/







void EXTI3_IRQHandler(void) {
    HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_3);
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xSemaphoreGiveFromISR(sem_btn0, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}


void EXTI4_IRQHandler(void) {
    HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_4);
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xSemaphoreGiveFromISR(sem_btn3, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}


void EXTI9_5_IRQHandler(void) {
    HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_5);
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xSemaphoreGiveFromISR(sem_btn2, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}


void EXTI15_10_IRQHandler(void) {
    HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_15);
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xSemaphoreGiveFromISR(sem_btn1, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_I2C2_Init();
  MX_USART2_UART_Init();
  MX_TIM1_Init();
  /* USER CODE BEGIN 2 */

  sem_btn0 = xSemaphoreCreateBinary();
  sem_btn1 = xSemaphoreCreateBinary();
  sem_btn2 = xSemaphoreCreateBinary();
  sem_btn3 = xSemaphoreCreateBinary();


  I2C_txQueue = xQueueCreate(4, sizeof(i2cQueue_t));
  I2C_rxQueue = xQueueCreate(4, sizeof(uint8_t) * 8);
  angleQueue = xQueueCreate(2, sizeof(float));
  angleQueuePID  = xQueueCreate(1, sizeof(float));
  rtcQueue = xQueueCreate(2, sizeof(rtc_time_t));
  controlQueue = xQueueCreate(1, sizeof(ControlParams_t)); // de 1 elemento para mantener los ultimos valores

  xTaskCreate(I2C_controllerTask, "I2C_Guardian", 256, NULL, 2, NULL);
  xTaskCreate(pid_controlTask, "PID_Control", 256, NULL, 2, NULL);
  xTaskCreate(buttonHandlerTask, "Buttons", 256, NULL, 2, NULL);
  xTaskCreate(as5600_readerTask, "AS5600", 256, NULL, 1, NULL);
  xTaskCreate(menuTask, "Menu", 256, NULL, 1, NULL);
//  xTaskCreate(rtcSerialDebugTask, "RTC_Debug", 256, NULL, 1, NULL);

//     xTaskCreate(lcd_displayTask, "LCD", 256, NULL, 1, NULL);

  vTaskStartScheduler();

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }
  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief I2C2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C2_Init(void)
{

  /* USER CODE BEGIN I2C2_Init 0 */

  /* USER CODE END I2C2_Init 0 */

  /* USER CODE BEGIN I2C2_Init 1 */

  /* USER CODE END I2C2_Init 1 */
  hi2c2.Instance = I2C2;
  hi2c2.Init.ClockSpeed = 100000;
  hi2c2.Init.DutyCycle = I2C_DUTYCYCLE_2;
  hi2c2.Init.OwnAddress1 = 0;
  hi2c2.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c2.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c2.Init.OwnAddress2 = 0;
  hi2c2.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c2.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C2_Init 2 */

  /* USER CODE END I2C2_Init 2 */

}

/**
  * @brief TIM1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM1_Init(void)
{

  /* USER CODE BEGIN TIM1_Init 0 */

  /* USER CODE END TIM1_Init 0 */

  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};
  TIM_BreakDeadTimeConfigTypeDef sBreakDeadTimeConfig = {0};

  /* USER CODE BEGIN TIM1_Init 1 */

  /* USER CODE END TIM1_Init 1 */
  htim1.Instance = TIM1;
  htim1.Init.Prescaler = 71;
  htim1.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim1.Init.Period = 999;
  htim1.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim1.Init.RepetitionCounter = 0;
  htim1.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_PWM_Init(&htim1) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim1, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 0;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCNPolarity = TIM_OCNPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  sConfigOC.OCIdleState = TIM_OCIDLESTATE_RESET;
  sConfigOC.OCNIdleState = TIM_OCNIDLESTATE_RESET;
  if (HAL_TIM_PWM_ConfigChannel(&htim1, &sConfigOC, TIM_CHANNEL_2) != HAL_OK)
  {
    Error_Handler();
  }
  sBreakDeadTimeConfig.OffStateRunMode = TIM_OSSR_DISABLE;
  sBreakDeadTimeConfig.OffStateIDLEMode = TIM_OSSI_DISABLE;
  sBreakDeadTimeConfig.LockLevel = TIM_LOCKLEVEL_OFF;
  sBreakDeadTimeConfig.DeadTime = 0;
  sBreakDeadTimeConfig.BreakState = TIM_BREAK_DISABLE;
  sBreakDeadTimeConfig.BreakPolarity = TIM_BREAKPOLARITY_HIGH;
  sBreakDeadTimeConfig.AutomaticOutput = TIM_AUTOMATICOUTPUT_DISABLE;
  if (HAL_TIMEx_ConfigBreakDeadTime(&htim1, &sBreakDeadTimeConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM1_Init 2 */

  /* USER CODE END TIM1_Init 2 */
  HAL_TIM_MspPostInit(&htim1);

}

/**
  * @brief USART2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART2_UART_Init(void)
{

  /* USER CODE BEGIN USART2_Init 0 */

  /* USER CODE END USART2_Init 0 */

  /* USER CODE BEGIN USART2_Init 1 */

  /* USER CODE END USART2_Init 1 */
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 115200;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART2_Init 2 */

  /* USER CODE END USART2_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, LED_RANGO_Pin|IN1_Pin|IN2_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_RESET);

  /*Configure GPIO pins : LED_RANGO_Pin IN1_Pin IN2_Pin */
  GPIO_InitStruct.Pin = LED_RANGO_Pin|IN1_Pin|IN2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pin : PB0 */
  GPIO_InitStruct.Pin = GPIO_PIN_0;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pin : PA15 */
  GPIO_InitStruct.Pin = GPIO_PIN_15;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : PB3 PB4 PB5 */
  GPIO_InitStruct.Pin = GPIO_PIN_3|GPIO_PIN_4|GPIO_PIN_5;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* EXTI interrupt init*/
  HAL_NVIC_SetPriority(EXTI3_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(EXTI3_IRQn);

  HAL_NVIC_SetPriority(EXTI4_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(EXTI4_IRQn);

  HAL_NVIC_SetPriority(EXTI9_5_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(EXTI9_5_IRQn);

  HAL_NVIC_SetPriority(EXTI15_10_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);

}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

 /**
  * @brief  Period elapsed callback in non blocking mode
  * @note   This function is called  when TIM2 interrupt took place, inside
  * HAL_TIM_IRQHandler(). It makes a direct call to HAL_IncTick() to increment
  * a global variable "uwTick" used as application time base.
  * @param  htim : TIM handle
  * @retval None
  */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  /* USER CODE BEGIN Callback 0 */

  /* USER CODE END Callback 0 */
  if (htim->Instance == TIM2) {
    HAL_IncTick();
  }
  /* USER CODE BEGIN Callback 1 */

  /* USER CODE END Callback 1 */
}

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/

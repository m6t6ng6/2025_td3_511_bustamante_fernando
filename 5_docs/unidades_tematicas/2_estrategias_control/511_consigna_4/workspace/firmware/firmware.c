#include <stdio.h>
#include <string.h>
#include <math.h>
#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "hardware/i2c.h"
#include "hardware/gpio.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include "lcd.h"
#include "eeprom.h"
#include "ds3231.h"
#include "mcp4725.h"

/* ----------------- CONSTANTES ---------------- */

// Maximo valor de cuenta para el semaforo
#define MAX_COUNT       10000
#define PIN_LCD_SDA     20      // GP20 (pin 26 físico)
#define PIN_LCD_SCL     21      // GP21 (pin 27 físico)
#define PIN_RTC_SDA     20
#define PIN_RTC_SCL     21  
#define PIN_ENC_CLK     12      // GP12 (pin 16 físico)
#define PIN_ENC_DT      11      // GP11 (pin 15 físico)
#define PIN_ENC_SW      10      // GP10 (pin 14 físico)
#define PIN_BTN_CONFIG  9
#define PIN_LED_MAX     8
#define PIN_LED_MIN     7
#define PIN_ADC0        26
#define PIN_ADC1        27

#define LCD_DIR     0x27
#define RTC_DIR     0x68
#define EEPROM_DIR  0x57
#define DAC_DIR     0x60

// Constantes del sistema
#define SHUNT_RESISTANCE 10.0f  	  // 10 ohms
#define MAX_CURRENT_MA 250.0f    	  // 250 mA máximo
#define CURRENT_RESOLUTION_MA 0.1f 	  // Resolución de 0.1 mA
#define MAX_VOLTAGE_SENSOR 0.12f  	  // 0.12V representa 12V
#define VOLTAGE_SCALE_FACTOR 100.0f 	  // 0.12V -> 12V (x100)
#define VOLTAGE_RESOLUTION_V 0.1f  	  // Resolución de 0.1 V
#define DEBOUNCE_US 7000    // Tiempo antirrebote en microsegundos

// Constantes de configuración
#define NUM_RESISTENCIAS 3
#define TOTAL_PANTALLAS (5 + NUM_RESISTENCIAS)
#define MAX_I_mA_Value   250
#define MAX_V_mV_Value   120       // 12.0 V max
#define MAX_tiempo_mS_Value 120    // 2 minutos maximo
#define MAX_Resistencia_Value 9999

#define TIEMPO_REFRESH_LCD_MS  500  // Tiempo de refresco de LCD en MODO ACTIVO

/* ----------------- VARIABLES Y ESTRUCTURAS ----------------- */

// Variables de antirrebote
volatile uint64_t last_clk_time = 0;
volatile uint64_t last_sw_time = 0;
volatile bool last_clk_state = 1;  // estado anterior de CLK (Encoder)
volatile bool last_sw_state = 1;   // estado anterior de SW (Encoder)
static uint64_t last_config_time = 0;
static bool last_config_state = 1;  // Se asume que el botón está en reposo (pull-up -> alto)

ds3231_rtc_t rtc;       // Variable global del RTC
ds3231_datetime_t dt;   // Datetime global del RTC
mcp4725_t dac;

// Índice y valor de la resistencia actual
volatile uint8_t indice_R_actual = 0;
volatile uint32_t R_actual = 0;

typedef struct {
    char textoLCD[4][21];
} lcd_data_t;

typedef struct {
    eeprom_data_type_t tipo_dato;    // Setpoint o Alarma
    eeprom_data_id_t id;             // Vmax, Imax, R1-R10, etc.
    ds3231_datetime_t timestamp;     // Fecha y hora del evento
    float valor;                     // Valor del setpoint o de la alarma
} eeprom_data_t;

typedef struct {
    float Vmax;
    float Imax;
    float Vmin;         
    float Imin;
    uint32_t tiempo_ms;
    uint32_t R_setpoints[NUM_RESISTENCIAS];
} setpoint_data_t;

setpoint_data_t setpoint_global;

typedef struct {
    float Iload_ma;    // Corriente en mA
    float Vin_v;     // Tensión en V
    float Vshunt_v;   // Voltaje en la resistencia shunt
} sensado_data_t;

typedef struct {
    float Kp, Ki, Kd, Ts;
} pid_params_t;

typedef struct {
    float integral;
    float prev_error;
} pid_state_t;

/*------------- COLAS Y SEMAFOROS  -------------*/

QueueHandle_t Queue_Setpoints;
QueueHandle_t Queue_EEPROM;
QueueHandle_t Queue_EscribirLCD;
QueueHandle_t Queue_Sensado;
QueueHandle_t Queue_DAC;
SemaphoreHandle_t Sem_Bin_Select_Mas, Sem_Bin_Select_Menos, Sem_Bin_OK;
SemaphoreHandle_t Sem_Bin_Config, Sem_Bin_Memory;
SemaphoreHandle_t Sem_Bin_Resistencia, Sem_Bin_ReadyToRead;
SemaphoreHandle_t Sem_I2C0_Mutex;

/*------------- INTERRUPCIONES  -------------*/

// Interrupcion de giro encoder
void gpio_callback(uint gpio, uint32_t events) {
    uint64_t now = time_us_64();
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    if (gpio == PIN_ENC_CLK) {
        bool clk = gpio_get(PIN_ENC_CLK);
        bool dt  = gpio_get(PIN_ENC_DT);

        // Detectar flanco y hacer antirrebote por tiempo
        if (clk == 1 && last_clk_state == 0 && (now - last_clk_time) > 10000) {
            last_clk_time = now;

            if (dt != clk) {
                xSemaphoreGiveFromISR(Sem_Bin_Select_Mas, &xHigherPriorityTaskWoken);
            } else {
                xSemaphoreGiveFromISR(Sem_Bin_Select_Menos, &xHigherPriorityTaskWoken);
            }
        }

        last_clk_state = clk;
    }

    if (gpio == PIN_ENC_SW) {
        bool sw = gpio_get(PIN_ENC_SW);

        // Detectar flanco de bajada con antirrebote
        if (sw == 0 && last_sw_state == 1 && (now - last_sw_time) > 25000) {
            last_sw_time = now;
            xSemaphoreGiveFromISR(Sem_Bin_OK, &xHigherPriorityTaskWoken);
        }

        last_sw_state = sw;
    }

    if (gpio == PIN_BTN_CONFIG) {
        uint64_t now = time_us_64();
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;

        // Detectar flanco de bajada con antirrebote
        if ((now - last_config_time) > 20000) {
            last_config_time = now;
            xSemaphoreGiveFromISR(Sem_Bin_Config, &xHigherPriorityTaskWoken);
        }

    }

    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}


/*-------------------FUNCIONES------------------*/

void mostrar_ultimos_setpoints(void) {
    lcd_data_t lcd_buffer;
    eeprom_data_t dato;
    uint8_t buffer[sizeof(eeprom_data_t)];
    uint16_t addr = EEPROM_ADDR_SETPOINTS;

    for (int i = 0; i < 12; i++) {
        eeprom_read_data(i2c_default, addr, buffer, sizeof(buffer));
        memcpy(&dato, buffer, sizeof(dato));

        // Mostrar en LCD
        snprintf(lcd_buffer.textoLCD[0], 21, "ID: %d Tipo: %d", dato.id, dato.tipo_dato);
        snprintf(lcd_buffer.textoLCD[1], 21, "Valor: %.2f", dato.valor);
        snprintf(lcd_buffer.textoLCD[2], 21, "%02d/%02d/%04d", dato.timestamp.day, dato.timestamp.month, dato.timestamp.year);
        snprintf(lcd_buffer.textoLCD[3], 21, "%02d:%02d:%02d", dato.timestamp.hour, dato.timestamp.minutes, dato.timestamp.seconds);

        xQueueSend(Queue_EscribirLCD, &lcd_buffer, portMAX_DELAY);
        vTaskDelay(pdMS_TO_TICKS(2000));  // 2 segundos por pantalla

        addr += sizeof(eeprom_data_t);
    }
}

// Función auxiliar: calcular columna del cursor según parámetro
int columna_cursor(int pantalla_actual, int digit_selected) {
    if (pantalla_actual == 0 || pantalla_actual == 2) return 3 - 2*digit_selected;      // Vmax/Vmin XX.X
    if (pantalla_actual == 1 || pantalla_actual == 3) return 2 - digit_selected;        // Imax/Imin XXX
    if (pantalla_actual == 4) return 2 - digit_selected;                                // Tiempo XXX
    return 3 - digit_selected;                                                          // Resistencias 7 dígitos
}



// Función auxiliar: actualizar valor según giro del encoder
void actualizar_valor(int *valor, int max_val, int digit, bool incrementar) {
    // VALOR sera el puntero a las variables de cada modo de configuracion
    // MAX_VAL es el valor maximo para ese parametro
    // DIGIT es el digito que se aumenta o decrementa
    // INCREMENTAR es booleano
    int factor = 1;
    for(int i = 0; i < digit; i++) factor *= 10;

    if(incrementar) {
        *valor += factor;
        if(*valor > max_val) *valor = max_val;
    } else {
        *valor -= factor;
        if(*valor < 0) *valor = 0;
    }
}

// Funcion para limites MAX y MIN
alarma_flag_t  check_limits (setpoint_data_t *setpoint, sensado_data_t *measurement, sensado_data_t *alarma_measurement) {
    alarma_flag_t alarma = ALARMA_NONE;

    // Set alarmas
    if (measurement->Vin_v > setpoint->Vmax)        alarma |= ALARMA_VMAX; 
    if (measurement->Vin_v < setpoint->Vmin)        alarma |= ALARMA_VMIN;
    if (measurement->Iload_ma > setpoint->Imax)     alarma |= ALARMA_IMAX;
    if (measurement->Iload_ma < setpoint->Imin)     alarma |= ALARMA_IMIN;

    *alarma_measurement=*measurement;
    return alarma;
}



/*------------------- TAREAS ---------------------*/

void task_Control (void *pvParameters) {
    
    //pid_params_t pid = { .Kp = 1.85f, .Ki = 0.90f, .Kd = 0.0f, .Ts = 0.07f };
    pid_params_t pid = { .Kp = 1.85f, .Ki = 3.22f, .Kd = 0.0f, .Ts = 0.1f };
    pid_state_t pid_state = {0};
    setpoint_data_t setpoint;
    sensado_data_t measurement, alarma_measurement;
    eeprom_data_t alarma_eeprom;
    lcd_data_t buffer_lcd;
    alarma_flag_t alarma_flags;
    float dac_out, error, error_anterior;

    TickType_t last_lcd_update = 0;
    TickType_t last_res_update = 0;   // Control de tiempo para Sem_Resistencia
    TickType_t last_auxdac_update = 0;
    TickType_t last_alarmaeeprom_update = 0;
    TickType_t alarma_timer = 0;
    
    bool alerta = false;
    float valorM=0;

    while(1) {
        /*  ----------- Ajustes PID --------------
        
        if(xSemaphoreTake(Sem_Bin_Select_Mas, 0) == pdTRUE)
            pid.Kp = pid.Kp + 0.01;
        if(xSemaphoreTake(Sem_Bin_Select_Menos, 0) == pdTRUE)
            pid.Kp = pid.Kp - 0.01; */
/* 
        if(xSemaphoreTake(Sem_Bin_Select_Mas, 0) == pdTRUE)
            pid.Ki = pid.Ki + 0.02;
        if(xSemaphoreTake(Sem_Bin_Select_Menos, 0) == pdTRUE)
            pid.Ki = pid.Ki - 0.02; 
 */
        // Esperar nueva medición
        if(xQueueReceive(Queue_Sensado, &measurement, 0) == pdTRUE) {
            // Calcular corriente deseada: I = V_max / R
            float I_target;
            if (R_actual > 0) {
                I_target = (measurement.Vin_v / R_actual); // I_target en A
            } else {
                I_target = 0;
            }
            float Vshunt_target = I_target * 10.0f; // Vshunt [V] = I_target [A] * 10ohm
            
            // Control PID - Sobre la tension Vshunt
            error = Vshunt_target - measurement.Vshunt_v;

            // Integración con anti-windup
            float u_unsat = pid.Kp*error + pid.Ki*pid_state.integral; // previo a saturar
            float output = u_unsat;  // se saturará más abajo

            // Anti-windup condicional
            bool saturado_arriba = (u_unsat > 5.0f);
            bool saturado_abajo  = (u_unsat < 0.0f);

            // Integro solo si no esta saturado o error lleva hacia adentro
            if (!( (saturado_arriba && error > 0.0f) || (saturado_abajo && error < 0.0f) )) {
                pid_state.integral += error * pid.Ts;
            }

            // Limitante integral
            if (pid_state.integral > 5.00f) pid_state.integral = 5.00f;
            if (pid_state.integral < -5.00f) pid_state.integral = -5.00f;

            // Calculo sistema PI
            output =  pid.Kp * error + pid.Ki * pid_state.integral;
            
            pid_state.prev_error = error;                
            // Limito señal DAC
            if (output < 0.0f) output = 0.0f;
            if (output > 5.0f) output = 5.0f;
            // Enviar valor DAC a cola
            dac_out = output;
            xQueueSend(Queue_DAC, &dac_out, portMAX_DELAY);
            
            
            TickType_t now = xTaskGetTickCount();

            // Dar semáforo a task_Resistencia según tiempo configurado
            if ((now - last_res_update) >= pdMS_TO_TICKS(setpoint_global.tiempo_ms)) {
                xSemaphoreGive(Sem_Bin_Resistencia);
                last_res_update = now;
                pid_state.integral = 0.0f; // Resetear integral al cambiar setpoint
                pid_state.prev_error = 0.0f;
            }
            
            // ------------- ALARMAS - Si hay alerta guarda en EEPROM --------------
            alarma_flags = check_limits(&setpoint_global, &measurement, &alarma_measurement);

            // Encendido LEDs
            gpio_put(PIN_LED_MAX, (alarma_flags & (ALARMA_VMAX | ALARMA_IMAX)) != 0);
            gpio_put(PIN_LED_MIN, (alarma_flags & (ALARMA_VMIN | ALARMA_IMIN)) != 0);

            if (alarma_flags) {
                if (alarma_timer == 0) {
                    alarma_timer = now; // arranca el conteo
                } else if ((now - alarma_timer) >= pdMS_TO_TICKS(2000)) { // 1 seg mínimo de alarma para activar
                    if ((R_actual > 0) && (measurement.Vin_v > 1) && ((now - last_alarmaeeprom_update) >= pdMS_TO_TICKS(5000))) {
                    // Solo manda alarma a EEPROM si R>0 y Vin>1
                        alarma_eeprom.tipo_dato = EEPROM_DATA_ALARMA;

                        if (xSemaphoreTake(Sem_I2C0_Mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                            ds3231_get_datetime(&dt, &rtc);
                            alarma_eeprom.timestamp = dt;
                            xSemaphoreGive(Sem_I2C0_Mutex);
                        }
                    
                        // Manda todas las alarmas
                        if (alarma_flags & ALARMA_VMAX) { 
                            alarma_eeprom.id = ID_VMAX; 
                            alarma_eeprom.valor = alarma_measurement.Vin_v; 
                            xQueueSend(Queue_EEPROM, &alarma_eeprom, 0); }
                        if (alarma_flags & ALARMA_IMAX) { 
                            alarma_eeprom.id = ID_IMAX; 
                            alarma_eeprom.valor = alarma_measurement.Iload_ma; 
                            xQueueSend(Queue_EEPROM, &alarma_eeprom, 0); }
                        if (alarma_flags & ALARMA_VMIN) { 
                            alarma_eeprom.id = ID_VMIN; 
                            alarma_eeprom.valor = alarma_measurement.Vin_v; 
                            xQueueSend(Queue_EEPROM, &alarma_eeprom, 0); }
                        if (alarma_flags & ALARMA_IMIN) { 
                            alarma_eeprom.id = ID_IMIN; 
                            alarma_eeprom.valor = alarma_measurement.Iload_ma; 
                            xQueueSend(Queue_EEPROM, &alarma_eeprom, 0); }

                        last_alarmaeeprom_update = now;
                    }
                }
            } else {
                alarma_timer = 0; // resetea el timer si no hay alarma
            }

            // -------- LCD - Medicion en curso -----------
            if (((now - last_lcd_update)  >= pdMS_TO_TICKS(TIEMPO_REFRESH_LCD_MS)) && ((now - last_alarmaeeprom_update)  >= pdMS_TO_TICKS(1000))) {
                lcd_show_cursor(false,false);
                snprintf(buffer_lcd.textoLCD[0], 21, "%-20s", "Medicion en curso");
                //snprintf(buffer_lcd.textoLCD[1], 21, "R: %-4d OHM I:%1.2f", R_actual, pid.Ki);
                snprintf(buffer_lcd.textoLCD[1], 21, "R: %-4d OHM   ", R_actual);
                snprintf(buffer_lcd.textoLCD[2], 21, "V entrada: %-5.2f V", measurement.Vin_v);
                snprintf(buffer_lcd.textoLCD[3], 21, "Corriente: %-4d mA", (int)measurement.Iload_ma);
                //lcd_clear();
                xQueueSend(Queue_EscribirLCD, &buffer_lcd, 0);
                last_lcd_update = now;
            }

        }
        
        // Frecuencia de calculo -> 1ms -> 1KHz
        vTaskDelay(pdMS_TO_TICKS(10)); 
    }
}

void task_Sensado(void *pvParameters) {
    sensado_data_t measurement;
    const uint8_t num_samples = 10;
    uint32_t voltage_in_sum;
    uint32_t voltage_shunt_sum;
    float voltage_shunt_adc, voltage_shunt, voltage_sensor_adc, voltage_sensor;
    
    while(1) {
        if (xSemaphoreTake(Sem_Bin_ReadyToRead,portMAX_DELAY)==pdTRUE){
            voltage_shunt_sum = 0;
            voltage_in_sum = 0;
            
            // Da semaforo apenas lo toma
            xSemaphoreGive (Sem_Bin_ReadyToRead);
            // Toma múltiples muestras para promediar
            for(uint8_t i = 0; i < num_samples; i++) {
                // Lee corriente (pin 31 - ADC0)
                adc_select_input(0); // ADC0
                voltage_shunt_sum += adc_read();
                
                // Lee tensión (pin 32 - ADC1)
                adc_select_input(1); // ADC1
                voltage_in_sum += adc_read();
                
                vTaskDelay(pdMS_TO_TICKS(1)); // Pequeño delay entre lecturas
            }
            
            // Promedia las lecturas
            uint16_t voltage_shunt_raw = voltage_shunt_sum / num_samples;
            uint16_t voltage_raw = voltage_in_sum / num_samples;
            
            // --- CONVERSION ---
            
            // VSHUNT Y Icarga
            voltage_shunt_adc = (voltage_shunt_raw  * 3.3f) / 4095.0f; // Ajuste por tensiones en ADC 
            voltage_shunt = voltage_shunt_adc * (2.5f / 3.3f);
            measurement.Vshunt_v = voltage_shunt;
            measurement.Iload_ma = (voltage_shunt / SHUNT_RESISTANCE) * 1000.0f; // A -> mA

            // VIN
            voltage_sensor_adc = (voltage_raw * 3.3f) / 4095.0f;
            voltage_sensor = voltage_sensor_adc * (12.0f / 3.3f); 
            measurement.Vin_v = voltage_sensor;
            
            // Envia por la cola
            xQueueSend(Queue_Sensado, &measurement, portMAX_DELAY);
            
            // Frecuencia de muestro -> 10ms -> 100Hz
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }
}

void task_DAC(void *pvParameters) {
    float dac_in;
    while(1) {
        if (xQueueReceive(Queue_DAC, &dac_in, portMAX_DELAY) == pdTRUE) {
            xSemaphoreTake(Sem_I2C0_Mutex, portMAX_DELAY);
            mcp4725_set_voltage(&dac, dac_in, MCP4725_RegisterMode, MCP4725_PowerDown_OFF);
            xSemaphoreGive(Sem_I2C0_Mutex);
        }
    }
}

void task_Config(void *params) {
    int pantalla_actual = 0;
    int total_parametros = 15;
    int digit_selected = 0;
    int last_digit_selected = -1;
    bool pressed = false;
    bool cursor_visible = false;
    TickType_t last_cursor_time = 0;
    // Variables de cada parametro
    int V_max_mV=0, I_max_mA=0, V_min_mV=0, I_min_mA=0, tiempo_seg=0;
    int valores_res[NUM_RESISTENCIAS] = {0};
    // Punteros a cada parámetro (pantalla_actual) 
    int* ptr_valores[TOTAL_PANTALLAS];
    // Limites máximos
    int maximos[TOTAL_PANTALLAS];

    int num_digitos_resistencia = 4; 
    const int potencias[7] = {1,10,100,1000,10000,100000,1000000};

    lcd_data_t lcd_buffer;
    eeprom_data_t dato_eeprom;
    dato_eeprom.tipo_dato = EEPROM_DATA_SETPOINT;

    // Inicialización de valores de parametros
    ptr_valores[0] = &V_max_mV;     maximos[0] = MAX_V_mV_Value;
    ptr_valores[1] = &I_max_mA;     maximos[1] = MAX_I_mA_Value;
    ptr_valores[2] = &V_min_mV;     maximos[2] = MAX_V_mV_Value;
    ptr_valores[3] = &I_min_mA;     maximos[3] = MAX_I_mA_Value;
    ptr_valores[4] = &tiempo_seg;   maximos[4] = MAX_tiempo_mS_Value;
    for(int i=0; i<NUM_RESISTENCIAS; i++){
        ptr_valores[5 + i] = &valores_res[i];   maximos[5 + i] = MAX_Resistencia_Value;
    }

    while(1) {

        // Tomo semaforo MODO CONFIGURACION
        if (xSemaphoreTake(Sem_Bin_Config, portMAX_DELAY) == pdTRUE){
            // Pausar MODO ACTIVO
            xSemaphoreTake(Sem_Bin_ReadyToRead, 0);

            while(pantalla_actual < TOTAL_PANTALLAS){
                char linea[4][21];

                // MUESTRA CONFIGURACION DEL PARAMETRO
                if(pantalla_actual < 5){

                        // 🔵 Encender LED según el parámetro que estoy editando
                    if (pantalla_actual == 0 || pantalla_actual == 1) {
                        gpio_put(PIN_LED_MAX, 1);
                    }
                    else if (pantalla_actual == 2 || pantalla_actual == 3) {
                        gpio_put(PIN_LED_MIN, 1);
                    }

                    // Linea 0
                    switch(pantalla_actual){
                        case 0: snprintf(linea[0], 21, "CONFIG V MAX"); break;
                        case 1: snprintf(linea[0], 21, "CONFIG I MAX"); break;
                        case 2: snprintf(linea[0], 21, "CONFIG V MIN"); break;
                        case 3: snprintf(linea[0], 21, "CONFIG I MIN"); break;
                        case 4: snprintf(linea[0], 21, "CONFIG TIEMPO"); break;
                    }
                    // Linea 1
                    if(pantalla_actual==0 || pantalla_actual==2)
                        snprintf(linea[1], 21, "%2d.%1d V", *ptr_valores[pantalla_actual]/10, *ptr_valores[pantalla_actual]%10);
                    else if(pantalla_actual==1 || pantalla_actual==3)
                        snprintf(linea[1], 21, "%03d mA", *ptr_valores[pantalla_actual]);
                    else
                        snprintf(linea[1], 21, "%03d seg", *ptr_valores[pantalla_actual]);
                } 
                else {
                    int res_idx = pantalla_actual - 5;
                    snprintf(linea[0], 21, "CONFIG R%2d", res_idx + 1);
                    snprintf(linea[1], 21, "%04d OHM", *ptr_valores[pantalla_actual]);
                }
                // Linea 2 y 3
                linea[2][0] = '\0';
                linea[3][0] = '\0';

                for(int i=0; i<4; i++)     snprintf(lcd_buffer.textoLCD[i], 21, "%-20s", linea[i]);
                // Envío a imprimir el parametro actual
                xQueueSend(Queue_EscribirLCD, &lcd_buffer, portMAX_DELAY);

                // volver a ubicar cursor si corresponde
                if(cursor_visible){
                    int col = columna_cursor(pantalla_actual, digit_selected);
                    lcd_set_cursor(1, col);
                    lcd_show_cursor(true, true);
                }

                // -- CURSOR --
                if(cursor_visible && digit_selected != last_digit_selected){
                    int col = columna_cursor(pantalla_actual, digit_selected);
                    lcd_set_cursor(1, col);
                    last_cursor_time = xTaskGetTickCount();
                    lcd_show_cursor(true,true);   // solo ON si estaba en modo visible
                    last_digit_selected = digit_selected;
                }

                // -- GIRO DE ENCODER --
                // Actualiza el valor del parametro actual
                if(xSemaphoreTake(Sem_Bin_Select_Mas, 0) == pdTRUE)
                    actualizar_valor(ptr_valores[pantalla_actual], maximos[pantalla_actual], digit_selected, true);
                if(xSemaphoreTake(Sem_Bin_Select_Menos, 0) == pdTRUE)
                    actualizar_valor(ptr_valores[pantalla_actual], maximos[pantalla_actual], digit_selected, false);
                

                // -- BOTON DE ENCODER -- 
                if(gpio_get(PIN_ENC_SW) == 0 && !pressed){
                    pressed = true;
                    last_cursor_time = xTaskGetTickCount();
                }
                if(gpio_get(PIN_ENC_SW) == 1 && pressed){
                    pressed = false;
                    TickType_t elapsed = xTaskGetTickCount() - last_cursor_time;
                    
                    // CONFIRMA VALOR de parametro y pasa al siguiente
                    if(elapsed >= pdMS_TO_TICKS(1000)){

                        // Toma la hora del RTC
                        if (xSemaphoreTake(Sem_I2C0_Mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                            ds3231_get_datetime(&dt, &rtc);
                            dato_eeprom.timestamp = dt;
                            xSemaphoreGive(Sem_I2C0_Mutex);
                        }
                        dato_eeprom.id = (pantalla_actual < 5) ? pantalla_actual : ID_R1 + pantalla_actual - 5;
                        if (pantalla_actual==0 || pantalla_actual==2){
                            dato_eeprom.valor = (float)(*ptr_valores[pantalla_actual])/10.0f;
                        }else{
                            dato_eeprom.valor = (float)(*ptr_valores[pantalla_actual]);
                        }

                        // Guardar valor en EEPROM
                        xQueueSend(Queue_EEPROM, &dato_eeprom, portMAX_DELAY);
                        vTaskDelay(pdMS_TO_TICKS(2000));

                        // Apaga LEDs
                        gpio_put(PIN_LED_MAX, 0);
                        gpio_put(PIN_LED_MIN, 0);

                        pantalla_actual++;
                        digit_selected = 0;
                        last_digit_selected = -1;
                    } 

                    // CAMBIA DE DIGITO
                    else if(elapsed >= pdMS_TO_TICKS(100)){
                        // Paso entre dígitos
                        if ((pantalla_actual==0)||(pantalla_actual==2)) digit_selected=(digit_selected+1)%2;
                        else if ((pantalla_actual==1)||(pantalla_actual==3)) digit_selected=(digit_selected+1)%3;
                        else if (pantalla_actual==4) digit_selected=(digit_selected+1)%3;
                        else digit_selected=(digit_selected+1)% num_digitos_resistencia;

                        lcd_show_cursor(true, true);
                        cursor_visible = true;
                        last_digit_selected = -1; // forzar reubicación del cursor
                    }
                }

                vTaskDelay(pdMS_TO_TICKS(50));
            }
            
            // Guarda setpoints en SETPOINT_GLOBAL
            setpoint_global.Vmax=   V_max_mV/10.0f;
            setpoint_global.Imax=   (float)I_max_mA;
            setpoint_global.Vmin=   V_min_mV/10.0f;
            setpoint_global.Imin=   (float)I_min_mA;
            setpoint_global.tiempo_ms=  tiempo_seg*1000;
            for(int i=0;i<NUM_RESISTENCIAS;i++){
                setpoint_global.R_setpoints[i]=     valores_res[i];
            }   

            // Mensaje de CONFIG GUARDADA
            lcd_clear();
            snprintf(lcd_buffer.textoLCD[0],21,"CONFIG GUARDADA");
            for(int i=1;i<4;i++)    lcd_buffer.textoLCD[i][0]='\0';
            xQueueSend(Queue_EscribirLCD,&lcd_buffer,portMAX_DELAY);
            vTaskDelay(pdMS_TO_TICKS(2000));
            
            pantalla_actual=0;
            lcd_show_cursor(false,false);

            // Toma semaforo Config por si se apreto el boton
            xSemaphoreTake(Sem_Bin_Config, 0);
            // Sale de MODO CONFIGURACION
            xSemaphoreGive(Sem_Bin_ReadyToRead);
        }
    }
}


 void task_EEPROM(void *params) {
    eeprom_data_t dato;
    lcd_data_t lcd_text;
    static uint16_t addr_setpoints = EEPROM_ADDR_SETPOINTS;
    static uint16_t addr_alarmas   = EEPROM_ADDR_ALARMAS;
    static uint16_t addr_lecturas  = EEPROM_ADDR_LECTURAS;

    while (1) {
        if (xQueueReceive(Queue_EEPROM, &dato, portMAX_DELAY) == pdTRUE) {
            uint8_t buffer[sizeof(eeprom_data_t)];
            uint8_t buffer_lectura[sizeof(eeprom_data_t)];
            memcpy(buffer, &dato, sizeof(eeprom_data_t));

            uint16_t *addr_ptr = NULL;
            uint16_t addr_max = 0;
            uint16_t addr_min = 0;

            switch (dato.tipo_dato) {
                case EEPROM_DATA_SETPOINT:
                    addr_ptr = &addr_setpoints;
                    addr_min = EEPROM_ADDR_SETPOINTS;
                    addr_max = EEPROM_ADDR_SETPOINTS + EEPROM_SIZE_SETPOINTS;
                    break;

                case EEPROM_DATA_ALARMA:
                    addr_ptr = &addr_alarmas;
                    addr_min = EEPROM_ADDR_ALARMAS;
                    addr_max = EEPROM_ADDR_ALARMAS + EEPROM_SIZE_ALARMAS;
                    break;

                case EEPROM_DATA_LECTURA:
                    addr_ptr = &addr_lecturas;
                    addr_min = EEPROM_ADDR_LECTURAS;
                    addr_max = EEPROM_ADDR_LECTURAS + EEPROM_SIZE_LECTURAS;
                    break;

                default:
                    continue;  // Tipo desconocido, no escribir
            }

            // Reinicio circular por zona
            if (*addr_ptr + sizeof(eeprom_data_t) > addr_max) {
                *addr_ptr = addr_min;  // reiniciar a comienzo de la zona
            }

            // Toma Semaforo Mutex para escribir EEPROM
            if (xSemaphoreTake(Sem_I2C0_Mutex, pdMS_TO_TICKS(500)) == pdTRUE) {
                //Escribe EEPROM
                eeprom_write_data(i2c_default, *addr_ptr, buffer, sizeof(buffer));
                //Lee EEPROM para ver si leyo OK
                eeprom_read_data(i2c_default, *addr_ptr, buffer_lectura, sizeof(buffer));
                // Entrega Semaforo Mutex
                xSemaphoreGive(Sem_I2C0_Mutex);      
                *addr_ptr += sizeof(eeprom_data_t);
            } 
            
            memcpy(&dato, buffer_lectura, sizeof(dato));

            // Armar el contenido a mostrar
            if (dato.tipo_dato == EEPROM_DATA_SETPOINT){
                snprintf(lcd_text.textoLCD[0], 21, "GUARDADO - ID: %d  ", dato.id, dato.tipo_dato);
                switch (dato.id){            
                    case 0:     snprintf(lcd_text.textoLCD[1], 21, "Valor: %.1f V", dato.valor); break;
                    case 1:     snprintf(lcd_text.textoLCD[1], 21, "Valor: %d mA", (int)dato.valor); break;
                    case 2:     snprintf(lcd_text.textoLCD[1], 21, "Valor: %.1f V", dato.valor); break;
                    case 3:     snprintf(lcd_text.textoLCD[1], 21, "Valor: %d mA", (int)dato.valor); break;
                    case 4:     snprintf(lcd_text.textoLCD[1], 21, "Valor: %d seg", (int)dato.valor); break;
                    default:    snprintf(lcd_text.textoLCD[1], 21, "Valor: %d OHM", (int)dato.valor); break;
                }
            } else if (dato.tipo_dato == EEPROM_DATA_ALARMA)
            {
                snprintf(lcd_text.textoLCD[0], 21, "%-20s", "ALARMA");
                switch (dato.id){
                    case 0:     snprintf(lcd_text.textoLCD[1], 21, "Valor: %.1f V", dato.valor); break;
                    case 1:     snprintf(lcd_text.textoLCD[1], 21, "Valor: %d mA", (int)dato.valor); break;
                    case 2:     snprintf(lcd_text.textoLCD[1], 21, "Valor: %.1f V", dato.valor); break;
                    case 3:     snprintf(lcd_text.textoLCD[1], 21, "Valor: %d mA", (int)dato.valor); break;
                }
            }
            
            snprintf(lcd_text.textoLCD[2], 21, "%02d/%02d/%04d", dato.timestamp.day, dato.timestamp.month, dato.timestamp.year);
            snprintf(lcd_text.textoLCD[3], 21, "%02d:%02d:%02d", dato.timestamp.hour, dato.timestamp.minutes, dato.timestamp.seconds);
            
            // Muestro parametro guardado en EEPROM
            lcd_clear();
            xQueueSend(Queue_EscribirLCD, &lcd_text, portMAX_DELAY);

        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void task_LCD (void *params) {
    lcd_data_t data_print_LCD;
    lcd_data_t prev_data = {0}; // Inicializo vacío
    while (1) {
        // Recibo colas con estructura
        if (xQueueReceive(Queue_EscribirLCD, &data_print_LCD, portMAX_DELAY) == pdTRUE) {
             
            // Toma el mutex antes de escribir en I2C (LCD)
            if (xSemaphoreTake(Sem_I2C0_Mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                for (int i = 0; i < 4; i++) {
                    if (strncmp(prev_data.textoLCD[i], data_print_LCD.textoLCD[i], 21) != 0) {
                        lcd_set_cursor(i, 0);
                        lcd_string(data_print_LCD.textoLCD[i]);
                        strncpy(prev_data.textoLCD[i], data_print_LCD.textoLCD[i], 21);
                    }
                }
                xSemaphoreGive(Sem_I2C0_Mutex);  // Liberar el mutex
            }
        }
        vTaskDelay(pdMS_TO_TICKS(20)); // pequeña pausa para evitar saturar
    }
}

void task_Resistencia(void *pvParameters) {
    while(1) {
        // Espera a que task_Config envie el semaforo
        if (xSemaphoreTake(Sem_Bin_Resistencia, portMAX_DELAY) == pdTRUE) {
            // Actualizar resistencia actual
            R_actual = setpoint_global.R_setpoints[indice_R_actual];

            // Avanzar al siguiente índice
            indice_R_actual++;
            if (indice_R_actual >= NUM_RESISTENCIAS) {
                indice_R_actual = 0; // volver a R1
            }
        }
    }
}

void task_Init(void *params) {

    // Inicialización ENCODER
    // CLK
    gpio_init(PIN_ENC_CLK);
    gpio_set_dir(PIN_ENC_CLK, GPIO_IN);
    gpio_pull_up(PIN_ENC_CLK);
    // DT
    gpio_init(PIN_ENC_DT);
    gpio_set_dir(PIN_ENC_DT, GPIO_IN);
    gpio_pull_up(PIN_ENC_DT);
    // SW
    gpio_init(PIN_ENC_SW);
    gpio_set_dir(PIN_ENC_SW, GPIO_IN);
    gpio_pull_up(PIN_ENC_SW);

    // Inicialización LEDs
    gpio_init(PIN_LED_MAX);
    gpio_set_dir(PIN_LED_MAX, GPIO_OUT);
    gpio_put(PIN_LED_MAX, 0);  // Apagar al inicio

    gpio_init(PIN_LED_MIN);
    gpio_set_dir(PIN_LED_MIN, GPIO_OUT);
    gpio_put(PIN_LED_MIN, 0);  // Apagar al inicio

    // Inicialización BOTON
    gpio_init(PIN_BTN_CONFIG);
    gpio_set_dir(PIN_BTN_CONFIG, GPIO_IN);
    gpio_pull_down(PIN_BTN_CONFIG);

    // IRQs
    gpio_set_irq_enabled_with_callback(PIN_ENC_CLK, GPIO_IRQ_EDGE_RISE, true, &gpio_callback);
    gpio_set_irq_enabled(PIN_ENC_SW, GPIO_IRQ_EDGE_FALL, true);
    gpio_set_irq_enabled(PIN_BTN_CONFIG, GPIO_IRQ_EDGE_RISE, true);

    // Inicialización I2C a 100KHz
    i2c_init(i2c0, 100000);

    // Inicialización ADC
    adc_init();
    adc_gpio_init(26);  // Configura GPIO26 como entrada analógica
    adc_select_input(0);  // Selecciona canal 0 (GPIO26)
    adc_gpio_init(27);  // Configura GPIO27 como entrada analógica
    adc_select_input(1);  // Selecciona canal 1 (GPIO27)


    //pwm_init_channel(0);

    // Inicialización LCD
    gpio_set_function(PIN_LCD_SDA, GPIO_FUNC_I2C);
    gpio_set_function(PIN_LCD_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(PIN_LCD_SDA);
    gpio_pull_up(PIN_LCD_SCL);
    // Inicializa el LCD con el I2C0 y la direccion de 7 bits 0x27
    lcd_init(i2c0, LCD_DIR);

    // Inicialización DAC
    //dac.i2c_port = i2c0;
    //dac.i2c_addr = DAC_DIR;

    mcp4725_begin(&dac, DAC_DIR, i2c0, 100, PIN_LCD_SDA, PIN_LCD_SCL, 1000);

    mcp4725_set_reference_voltage(&dac, 5);
    mcp4725_set_voltage(&dac, 5, MCP4725_RegisterMode, MCP4725_PowerDown_OFF);
    
    // Inicialización RTC */
    //ds3231_init(i2c0, PIN_RTC_SDA, PIN_RTC_SCL, &rtc);
    rtc.i2c_port = i2c0;
    rtc.i2c_addr = DS3231_I2C_ADDRESS;
    ds3231_get_datetime(&dt, &rtc);

    if (dt.year <= 2002){
        dt.seconds = 0;
        dt.minutes = 0;
        dt.hour = 21;
        dt.day = 12;
        dt.month = 8;
        dt.year = 2025;
        dt.dotw = 1;
        ds3231_set_datetime(&dt, &rtc);
    }

    
    // Creación de colas y semaforos
    Queue_EscribirLCD   = xQueueCreate(1, sizeof(lcd_data_t));
    //Queue_Setpoints     = xQueueCreate(1, sizeof(setpoint_data_t));
    Queue_EEPROM        = xQueueCreate(1, sizeof(eeprom_data_t));
    Queue_Sensado       = xQueueCreate(5, sizeof(sensado_data_t));
    Queue_DAC           = xQueueCreate(5, sizeof(float));
    Sem_Bin_Select_Mas   = xSemaphoreCreateBinary();
    Sem_Bin_Select_Menos = xSemaphoreCreateBinary();
    Sem_Bin_OK           = xSemaphoreCreateBinary();
    Sem_Bin_Config       = xSemaphoreCreateBinary();
    Sem_Bin_ReadyToRead  = xSemaphoreCreateBinary();
    Sem_Bin_Resistencia  = xSemaphoreCreateBinary();
    Sem_I2C0_Mutex       = xSemaphoreCreateMutex();
    //Sem_Config_Mutex     = xSemaphoreCreateMutex();

    // Setpoint de ejemplo
    setpoint_global.Vmax = 11;
    setpoint_global.Imax = 240;
    setpoint_global.Vmin = 1;
    setpoint_global.Imin = 3;
    setpoint_global.tiempo_ms = 10000;
    setpoint_global.R_setpoints[0] = 50;
    setpoint_global.R_setpoints[1] = 100;
    setpoint_global.R_setpoints[2] = 150;

    // Ingresa en modo Config
    xSemaphoreGive(Sem_Bin_Config);
    //xSemaphoreGive(Sem_Bin_ReadyToRead);
    // Elimino la tarea para liberar recursos
    vTaskDelete(NULL);
}


int main()
{
    stdio_init_all();
    // Creacion de tareas
    xTaskCreate(task_Init, "Init", configMINIMAL_STACK_SIZE, NULL, 5, NULL);
    xTaskCreate(task_Resistencia, "Resistencias", configMINIMAL_STACK_SIZE, NULL, 4, NULL);
    xTaskCreate(task_LCD, "LCD", configMINIMAL_STACK_SIZE, NULL, 3, NULL);
    xTaskCreate(task_EEPROM, "Eeprom", 2 * configMINIMAL_STACK_SIZE, NULL, 3, NULL);
    xTaskCreate(task_Config, "Config", 2 * configMINIMAL_STACK_SIZE, NULL, 2, NULL);
    xTaskCreate(task_Sensado, "Sensado", 1024, NULL, 1, NULL);
    xTaskCreate(task_Control, "Control", 1024, NULL, 1, NULL);
    xTaskCreate(task_DAC, "DAC", configMINIMAL_STACK_SIZE, NULL, 1, NULL);
        
    // Arranca el scheduler
    vTaskStartScheduler();
    while(1);
}
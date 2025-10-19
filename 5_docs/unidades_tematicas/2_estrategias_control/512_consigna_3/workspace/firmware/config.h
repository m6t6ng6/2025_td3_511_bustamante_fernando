#define CLK 19
#define DT 18
#define SW 20

#define I2C_PORT i2c0
#define I2C_SDA 16
#define I2C_SCL 17

#define PIN_TEMT6000 28

#define PIN_PWM 1

#define MAX_SET_POINT 1500
#define MIN_SET_POINT 100
#define MAX_RISE_TIME 10000 // 10 segundos
#define PWM_WRAP      4095

#define PIN_LED_RED 2

#define PIN_LED_GREEN 4

#define PIN_BTN 21

//==========================CONTROL=====================================

#define KP 0.07f
#define KI 0.60f
#define KD 0.006f

//===========================DEBUG======================================

// #define PRINT_VALUES_MODE
// #define DEBUG_CONTROL
// #define DEBUG_LEDS
// #define DEBUG_GET_LUX
// #define DEBUG_I2C
// #define DEBUG_LOGS
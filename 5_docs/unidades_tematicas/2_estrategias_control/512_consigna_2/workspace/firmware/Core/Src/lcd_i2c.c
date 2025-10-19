#include "lcd_i2c.h"
#include "FreeRTOS.h"
#include "queue.h"
#include "main.h"
#include <string.h>
#include "i2c_shared.h" //estructura compartida de archivos

extern QueueHandle_t I2C_txQueue;  //declarar de forma externa el handler de la cola

#define LCD_ADDR (0x27 << 1)
#define LCD_BACKLIGHT 0x08
#define LCD_ENABLE 0x04
#define LCD_COMMAND 0
#define LCD_DATA 1

static void lcd_i2c_send(uint8_t data) {
    i2cQueue_t tx;
    tx.device_id = LCD_ADDR;
    tx.reg = data;
    tx.type = TRANSMIT;
    tx.n_bytes = 0;
    tx.data = NULL;
    xQueueSend(I2C_txQueue, &tx, portMAX_DELAY);
 //   vTaskDelay(1); // Delay mínimo para asegurar el tiempo del LCD
}

static void lcd_write_4bits(uint8_t data) {
    lcd_i2c_send(data | LCD_BACKLIGHT | LCD_ENABLE);
    lcd_i2c_send(data | LCD_BACKLIGHT);
}

static void lcd_send(uint8_t value, uint8_t mode) {
    uint8_t high_nibble = value & 0xF0;
    uint8_t low_nibble = (value << 4) & 0xF0;

    lcd_write_4bits(high_nibble | (mode ? 0x01 : 0x00));
    lcd_write_4bits(low_nibble | (mode ? 0x01 : 0x00));
}

void lcd_send_command(uint8_t cmd) {
    lcd_send(cmd, LCD_COMMAND);
}

void lcd_send_data(uint8_t data) {
    lcd_send(data, LCD_DATA);
}

void lcd_send_string(char* str) {
    while (*str) {
        lcd_send_data((uint8_t)(*str++));
    }
}

void lcd_goto_XY(uint8_t row, uint8_t col) {
    uint8_t row_offsets[] = {0x00, 0x40, 0x14, 0x54};
    if (row > 3) row = 3;  // protección
    lcd_send_command(0x80 | (col + row_offsets[row]));
}


void lcd_clear(void) {
    lcd_send_command(0x01);
    vTaskDelay(2);
}

void lcd_init(void) {
    vTaskDelay(50);
    lcd_write_4bits(0x30);
    vTaskDelay(5);
    lcd_write_4bits(0x30);
    vTaskDelay(1);
    lcd_write_4bits(0x30);
    vTaskDelay(10);
    lcd_write_4bits(0x20); // 4-bit mode

    lcd_send_command(0x28); // 2 lines, 5x8 dots
    lcd_send_command(0x08); // Display off
    lcd_send_command(0x01); // Clear display
    vTaskDelay(2);
    lcd_send_command(0x06); // Entry mode set
    lcd_send_command(0x0C); // Display on, cursor off
}
void lcd_blink_on(void) {
    lcd_send_command(0x0F); // Display ON, Cursor ON, Blink ON
}

void lcd_blink_off(void) {
    lcd_send_command(0x0C); // Display ON, Cursor OFF, Blink OFF
}

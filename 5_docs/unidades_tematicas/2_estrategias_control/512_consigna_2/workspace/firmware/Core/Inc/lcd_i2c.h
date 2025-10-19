#ifndef LCD_I2C_H
#define LCD_I2C_H

#include <stdint.h>
#include <stddef.h>

void lcd_init(void);
void lcd_clear(void);
void lcd_send_command(uint8_t cmd);
void lcd_send_data(uint8_t data);
void lcd_send_string(char* str);
void lcd_goto_XY(uint8_t row, uint8_t col);
void lcd_blink_on(void);
void lcd_blink_off(void);

#endif // LCD_I2C_H

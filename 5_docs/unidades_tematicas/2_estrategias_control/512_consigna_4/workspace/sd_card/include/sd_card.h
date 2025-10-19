#ifndef SD_CARD_H
#define SD_CARD_H

#include "ff.h"
#include "diskio.h"
#include "pico/stdlib.h"
#include <stdio.h>

int8_t open_file_sd_card(FATFS *fs, FIL *file, const char *filename, BYTE mode);
bool sd_card_alive(void);
uint8_t sd_card_get_file_count(void);

#endif // SD_CARD_H
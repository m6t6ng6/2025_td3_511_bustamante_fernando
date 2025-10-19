#include "hardware/flash.h"
#include "hardware/sync.h"
#include "stdio.h"
#include <string.h>

#define FLASH_TARGET_OFFSET (2 * 1024 * 1024 - 4096)  // Último sector de 4 KB de la flash
#define MAX_LOG_ENTRIES 128
#define ENTRY_SIZE 64  // Tamaño fijo por entrada para facilidad

static char log_buffer[MAX_LOG_ENTRIES * ENTRY_SIZE] __attribute__((aligned(4)));

void read_all_logs();

void read_log(uint32_t num_log, char *log);

void save_log(const char *entry);

void erase_all_logs(void);

void over_write(const uint8_t *empty_data, size_t page_size);

void save_log_on(const char *entry, int i);

uint16_t read_log_u16(int i);

void save_u16_as_bytes(uint16_t value, int i);
#include <stdio.h>
#include "pico/stdlib.h"
#include "sd_card.h"

int main() {
    stdio_init_all();
    sleep_ms(2000); // Esperar para conectar terminal

    FATFS fs;
    FRESULT fr = f_mount(&fs, "", 1);
    if (fr != FR_OK) {
        printf("Error en f_mount: %d\n", fr);
        return 1;
    }

    DIR dir;
    FILINFO fno;
    fr = f_opendir(&dir, "/");
    if (fr != FR_OK) {
        printf("Error al abrir directorio raíz: %d\n", fr);
        return 1;
    }

    char c = 'a';
    while (c != 'q') {
        printf("Presiona 'q' para salir o cualquier otra tecla para continuar:\n");
        c = getchar_timeout_us(500000);
        if (c == 'q') break;
    }

    uint8_t index = 0;
    printf("Listado de archivos:\n");
    while (1) {
        fno.fname[0] = '\0'; // Limpiar el nombre del archivo
        fr = f_readdir(&dir, &fno);
        if (fr != FR_OK || fno.fname[0] == 0) break;
        printf(" - %s%s (%s)\n", 
            (fno.fattrib & AM_DIR) ? "[DIR] " : "", 
            fno.fname,
            fno.altname[0] ? fno.altname : "no 8.3"
        );
        sleep_ms(50); // Pausa para evitar saturar la salida
        index++;
    }

    f_closedir(&dir);
    return 0;
}

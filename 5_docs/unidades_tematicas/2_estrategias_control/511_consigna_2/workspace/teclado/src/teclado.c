//Funciones para la lectura del teclado
#include "teclado.h"
#include "pico/stdlib.h"

char escanear_teclado() {
    for (int f = 0; f < FIL; f++) {
        // Activar fila f
        for (int i = 0; i < FIL; i++)
            gpio_put(FILA_PINS[i], i == f ? 0 : 1);

        sleep_us(10);  // pequeña espera para estabilizar

        for (int c = 0; c < COL; c++) {
            if (gpio_get(COL_PINS[c])==0) {
                sleep_ms(200);  // debounce simple
                return teclas[f][c];
            }
        }
    }
    return 0;
}


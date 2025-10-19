//Funciones para la lectura del teclado
#ifndef TECLADO_H_
#define TECLADO_H_

#include <stdio.h>
#include <stdint.h>
#include "hardware/i2c.h"

#define FIL 4
#define COL 4

//Variables
extern const uint FILA_PINS[];
extern const uint COL_PINS[];
extern const char teclas[4][4];


char escanear_teclado(void);
#endif

# bmp280

Para agregar esta biblioteca en el proyecto, incluir en el `CMakeLists.txt` general lo siguiente:

```cmake
# Añadir la subcarpeta donde está la biblioteca BMP280
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/../bmp280 ${CMAKE_BINARY_DIR}/bmp280)
# Agrega dependencia al proyecto
target_link_libraries(firmware bmp280)
```

## Uso de la biblioteca

Una vez incluida la biblioteca con `#include "bmp280.h"` podemos hacer algo básico con el BMP280 usando:

```c
// Inicializa el BMP280 usando el I2C0
bmp280_init(i2c0);

// Obtiene parámetros de compensación
struct bmp280_calib_param params;
bmp280_get_calib_params(&params);

// Obtiene valores sin compensar
int32_t raw_temperature, raw_pressure;
bmp280_read_raw(&raw_temperature, &raw_pressure);

// Obtiene los valores compensados de temperatura y presión
float temperature = bmp280_convert_temp(raw_temperature, &params);
int32_t pressure = bmp280_convert_pressure(raw_pressure, raw_temperature, &params);
```

> :warning: La inicializacion del I2C de la Raspberry Pi Pico y los GPIO deben hacerse previamente.
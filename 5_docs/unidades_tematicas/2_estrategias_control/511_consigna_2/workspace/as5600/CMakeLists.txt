# Crear la biblioteca estática "as5600" con los archivos fuente
add_library(as5600 STATIC
    src/as5600.c
)

# Linkeo dependencias de la bibliotecas
target_link_libraries(as5600
    pico_stdlib
    hardware_i2c
    hardware_adc
    hardware_irq
)

# Incluir las cabeceras de la biblioteca
target_include_directories(as5600 PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/include
)
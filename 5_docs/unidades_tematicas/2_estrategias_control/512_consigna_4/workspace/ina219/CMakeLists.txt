# Crear la biblioteca estática "ina219" con los archivos fuente
add_library(ina219 STATIC
    src/ina219.c
)

# Linkeo dependencias de la bibliotecas
target_link_libraries(ina219
    pico_stdlib
    hardware_i2c
)

# Incluir las cabeceras de la biblioteca
target_include_directories(ina219 PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/include
)

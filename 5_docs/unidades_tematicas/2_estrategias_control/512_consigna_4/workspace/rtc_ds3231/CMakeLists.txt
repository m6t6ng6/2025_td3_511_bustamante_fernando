# Crear la biblioteca estática "rtc_ds3231" con los archivos fuente
add_library(rtc_ds3231 STATIC
    src/ds3231.c
)

# Linkeo dependencias de la bibliotecas
target_link_libraries(rtc_ds3231
    pico_stdlib
    hardware_i2c
)

# Incluir las cabeceras de la biblioteca
target_include_directories(rtc_ds3231 PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/include
)

# Crear la biblioteca estática "rtc" con los archivos fuente
add_library(rtc STATIC
    src/rtc.c
)

# Linkeo dependencias de la bibliotecas
target_link_libraries(rtc
    pico_stdlib
    hardware_i2c
)

# Incluir las cabeceras de la biblioteca
target_include_directories(rtc PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/include
)

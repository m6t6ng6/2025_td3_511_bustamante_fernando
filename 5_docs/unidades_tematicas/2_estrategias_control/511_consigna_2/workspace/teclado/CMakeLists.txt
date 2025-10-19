# Crear la biblioteca estática "teclado" con los archivos fuente
add_library(teclado STATIC
    src/teclado.c
)


# Linkeo dependencias de la bibliotecas
target_link_libraries(teclado
    pico_stdlib
    hardware_i2c
)

# Incluir las cabeceras de la biblioteca
target_include_directories(teclado PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/include
)
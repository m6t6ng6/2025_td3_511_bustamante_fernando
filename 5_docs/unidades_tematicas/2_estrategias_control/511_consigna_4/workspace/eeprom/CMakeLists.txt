# Creo biblioteca estática eeprom
add_library(eeprom STATIC src/eeprom.c)

# Incluyo headers propios
target_include_directories(eeprom PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Linkeo dependencias, agrego hardware_i2c y pico_stdlib (para pico/stdlib.h)
target_link_libraries(eeprom PUBLIC
    pico_stdlib
    hardware_i2c
)
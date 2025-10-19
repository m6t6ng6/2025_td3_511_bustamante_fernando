set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_library(ds1307 
    ds1307.c
)

target_include_directories(ds1307 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(
    ds1307
    hardware_i2c
    pico_stdlib
)
add_library(bh1750 
    bh1750.c
)

target_include_directories(bh1750 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(bh1750
    pico_stdlib
    hardware_i2c
)


add_library(temt6000 
    temt6000.c
)

target_include_directories(temt6000 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(temt6000
    pico_stdlib
    hardware_adc
)

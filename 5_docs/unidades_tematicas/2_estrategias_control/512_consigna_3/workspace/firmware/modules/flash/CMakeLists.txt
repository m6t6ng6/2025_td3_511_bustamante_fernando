add_library(flash 
    flash.c
)

target_include_directories(flash PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(flash
    hardware_flash
    pico_stdlib
)

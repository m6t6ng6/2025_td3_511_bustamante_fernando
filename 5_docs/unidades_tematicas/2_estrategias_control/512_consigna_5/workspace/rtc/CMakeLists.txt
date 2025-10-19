add_library(rtc STATIC
    src/rtc.c
)

target_link_libraries(rtc
    pico_stdlib
    hardware_i2c
)

target_include_directories(rtc PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/include
)
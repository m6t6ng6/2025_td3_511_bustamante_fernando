add_library(sd_card STATIC
    src/ff.c
    src/diskio.c
    src/ffunicode.c
    src/sd_card.c
)

target_link_libraries(sd_card
    pico_stdlib
    hardware_spi
)

target_include_directories(sd_card PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/include
)
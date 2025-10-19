add_library(fatfs_spi STATIC
    src/ff.c
    src/diskio.c
)

target_link_libraries(fatfs_spi
    pico_stdlib
    hardware_spi
)

target_include_directories(fatfs_spi PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/include
)
cmake_minimum_required(VERSION 3.12)
project(bmp280)

# Add BMP280 source files
add_library(bmp280 STATIC src/bmp280.c)

# Include BMP280 header files
target_include_directories(bmp280 PUBLIC include)

# Add dependencies
target_link_libraries(bmp280 PUBLIC pico_stdlib hardware_i2c)
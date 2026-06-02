cmake_minimum_required(VERSION 3.20.0)

# Cria variável que guarda o caminho para os drivers personalizados relativo ao diretório atual
set(CUSTOM_DRIVERS_DIR "${CMAKE_CURRENT_LIST_DIR}/custom_drivers")

list(APPEND ZEPHYR_EXTRA_MODULES "${CUSTOM_DRIVERS_DIR}")

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(hello_word_AS5300)

target_sources(app PRIVATE src/main.c)
target_include_directories(app PRIVATE "${CUSTOM_DRIVERS_DIR}/drivers/sensor/as5600")

add_subdirectory(src/kalman-filter-c)
target_link_libraries(app PRIVATE kalman)


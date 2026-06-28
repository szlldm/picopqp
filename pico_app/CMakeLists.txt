cmake_minimum_required(VERSION 3.12)

# MAIN Git verzió lekérdezése
execute_process(
	COMMAND git -C ${CMAKE_CURRENT_SOURCE_DIR} rev-parse HEAD # Teljes git verzió
	OUTPUT_VARIABLE MAIN_REPO_HASH
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

# PQP Git verzió lekérdezése
# ABSOLUTE path directory from file path relative
execute_process(
	COMMAND realpath ${CMAKE_CURRENT_SOURCE_DIR}/libpqp/libpqp.c
	OUTPUT_VARIABLE SYMLINK_TARGET
	OUTPUT_STRIP_TRAILING_WHITESPACE
)

get_filename_component(SYMLINK_DIR "${SYMLINK_TARGET}" DIRECTORY)

execute_process(
    COMMAND git -C ${SYMLINK_DIR} rev-parse HEAD # Teljes git verzió
    OUTPUT_VARIABLE PQP_REPO_HASH
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

if(DEFINED MAIN_REPO_HASH AND PQP_REPO_HASH)
	# 3. Első 8 karakter hex értékké alakítása
	string(SUBSTRING ${MAIN_REPO_HASH} 0 8 MAIN_GIT_VERSION)
	string(SUBSTRING ${PQP_REPO_HASH} 0 8 PQP_GIT_VERSION)

	add_compile_definitions(MAIN_GIT_HASH=0x${MAIN_GIT_VERSION})
	add_compile_definitions(PQP_GIT_HASH=0x${PQP_GIT_VERSION})

	message(STATUS "Main git hash: ${MAIN_GIT_VERSION} pqp git hash: ${PQP_GIT_VERSION}")
endif()

include_directories(${CMAKE_BINARY_DIR}/generated)

# Pull in SDK (must be before project)
include(pico_sdk_import.cmake)

project(app C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
add_compile_options(-O2)

set(APP_PATH ${PROJECT_SOURCE_DIR})

add_compile_definitions(SYS_CLK_KHZ=64000)
add_compile_definitions(PLL_SYS_VCO_FREQ_KHZ=1152000)
add_compile_definitions(PLL_SYS_POSTDIV1=6)
add_compile_definitions(PLL_SYS_POSTDIV2=3)

# Initialize the SDK
pico_sdk_init()

add_executable(pico_app)
add_compile_definitions(LIBPQP_IS_FIRMWARE)
file(GLOB_RECURSE LIBPQP_SOURCES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "libpqp/*.c")
target_sources(pico_app PRIVATE app.c main.c libpqp_foreign.c ${LIBPQP_SOURCES} can2040/can2040.c)
pico_set_linker_script(pico_app ${CMAKE_CURRENT_SOURCE_DIR}/app_memmap.ld)

# Pull in our pico_stdlib which pulls in commonly used features
target_link_libraries(pico_app pico_stdlib pico_multicore hardware_dma hardware_uart hardware_flash hardware_irq hardware_watchdog hardware_pwm hardware_exception pico_i2c_slave hardware_i2c)
pico_add_extra_outputs(pico_app)

# CAN2040 required includes:
include_directories(${PICO_SDK_PATH}/src/rp2_common/cmsis/stub/CMSIS/Core/Include/)
include_directories(${PICO_SDK_PATH}/src/rp2_common/cmsis/stub/CMSIS/Device/RaspberryPi/RP2040/Include/)

######################################
# TESTING:
pico_enable_stdio_usb(pico_app 1)
pico_enable_stdio_uart(pico_app 0)
######################################

add_compile_options(-Wall
        -Wno-format          # int != int32_t as far as the compiler is concerned because gcc has int32_t as long int
        -Wno-unused-function # we have some for the docs that aren't called
        -Wno-maybe-uninitialized
        )

target_link_options(pico_app PRIVATE
        -Xlinker --print-memory-usage
        )

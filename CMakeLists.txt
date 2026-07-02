cmake_minimum_required(VERSION 3.20)

set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/cmake/arm-none-eabi.cmake")

project(stm32f103-led-blink C ASM)

set(MCU_FLAGS
    -mcpu=cortex-m3
    -mthumb
)

add_executable(${PROJECT_NAME}.elf
    Core/Src/main.c
    Core/Src/uart.c
    Core/Src/dht11.c
    Core/Src/rtos.c
    startup_stm32f103xb.s
)

target_compile_options(${PROJECT_NAME}.elf PRIVATE
    ${MCU_FLAGS}
    -Wall
    -Wextra
    -ffunction-sections
    -fdata-sections
    -O0
    -g3
)

target_include_directories(${PROJECT_NAME}.elf PRIVATE
    Core/Inc
)

target_link_options(${PROJECT_NAME}.elf PRIVATE
    ${MCU_FLAGS}
    -T${CMAKE_CURRENT_SOURCE_DIR}/STM32F103C8Tx_FLASH.ld
    -Wl,--gc-sections
    -Wl,-Map=${PROJECT_NAME}.map
    -specs=nano.specs
    -specs=nosys.specs
)

add_custom_command(TARGET ${PROJECT_NAME}.elf POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:${PROJECT_NAME}.elf> ${PROJECT_NAME}.bin
    COMMAND ${CMAKE_OBJCOPY} -O ihex $<TARGET_FILE:${PROJECT_NAME}.elf> ${PROJECT_NAME}.hex
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE:${PROJECT_NAME}.elf>
)

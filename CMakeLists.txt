cmake_minimum_required(VERSION 3.20)

set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/cmake/arm-none-eabi.cmake")

project(stm32f103-led-blink C ASM)

set(MCU_FLAGS
    -mcpu=cortex-m3
    -mthumb
)

set(FIRMWARE_SOURCES
    Core/Src/main.c
    Core/Src/uart.c
    Core/Src/lora.c
    Core/Src/dht11.c
    Core/Src/rtos.c
    startup_stm32f103xb.s
)

function(add_lora_firmware target_name node_role)
    add_executable(${target_name}.elf ${FIRMWARE_SOURCES})

    target_compile_definitions(${target_name}.elf PRIVATE
        NODE_ROLE=${node_role}
    )

    target_compile_options(${target_name}.elf PRIVATE
        ${MCU_FLAGS}
        -Wall
        -Wextra
        -ffunction-sections
        -fdata-sections
        -O0
        -g3
    )

    target_include_directories(${target_name}.elf PRIVATE
        Core/Inc
    )

    target_link_options(${target_name}.elf PRIVATE
        ${MCU_FLAGS}
        -T${CMAKE_CURRENT_SOURCE_DIR}/STM32F103C8Tx_FLASH.ld
        -Wl,--gc-sections
        -Wl,-Map=${target_name}.map
        -specs=nano.specs
        -specs=nosys.specs
    )

    add_custom_command(TARGET ${target_name}.elf POST_BUILD
        COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:${target_name}.elf> ${target_name}.bin
        COMMAND ${CMAKE_OBJCOPY} -O ihex $<TARGET_FILE:${target_name}.elf> ${target_name}.hex
        COMMAND ${CMAKE_SIZE} $<TARGET_FILE:${target_name}.elf>
    )
endfunction()

add_lora_firmware(${PROJECT_NAME}-terminal ROLE_TERMINAL)
add_lora_firmware(${PROJECT_NAME}-gateway ROLE_GATEWAY)

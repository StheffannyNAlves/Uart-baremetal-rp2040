cmake_minimum_required(VERSION 3.12)

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR cortex-m0plus)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

project(BareMetal_Research C ASM)

set(CMAKE_C_COMPILER arm-none-eabi-gcc)
set(CMAKE_ASM_COMPILER arm-none-eabi-gcc)

set(CPU_FLAGS -mcpu=cortex-m0plus -mthumb)
set(LINKER_SCRIPT "${CMAKE_SOURCE_DIR}/linker/memmap.ld")

add_executable(firmware.elf
    ${CMAKE_SOURCE_DIR}/src/boot2_final.S
    ${CMAKE_SOURCE_DIR}/src/start.s
    ${CMAKE_SOURCE_DIR}/src/main.c
)

target_compile_options(firmware.elf PRIVATE
    ${CPU_FLAGS}
    -std=c11
    -Wall
    -O0
    -g
    -ffreestanding
    -ffunction-sections
    -fdata-sections
)

target_link_options(firmware.elf PRIVATE
    ${CPU_FLAGS}
    -T${LINKER_SCRIPT}
    -nostdlib
    -Wl,--gc-sections
)

target_link_libraries(firmware.elf PRIVATE gcc)

add_custom_command(TARGET firmware.elf POST_BUILD
    COMMAND arm-none-eabi-objcopy -O binary
            $<TARGET_FILE:firmware.elf>
            ${CMAKE_CURRENT_BINARY_DIR}/firmware.bin
    COMMAND python3 ${CMAKE_SOURCE_DIR}/tools/uf2conv.py
            -c ${CMAKE_CURRENT_BINARY_DIR}/firmware.bin
            -b 0x10000000
            -f 0xe48bff56
            -o ${CMAKE_CURRENT_BINARY_DIR}/firmware.uf2
    COMMENT ">>> Gerando firmware.bin e empacotando para UF2 <<<"
    VERBATIM
)
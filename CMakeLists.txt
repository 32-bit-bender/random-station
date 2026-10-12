cmake_minimum_required(VERSION 3.22)
project(random-station C ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CPU_FLAGS -mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard)
set(LL   ${CMAKE_SOURCE_DIR}/lib/stm32f4xx_hal_driver)
set(DEV  ${CMAKE_SOURCE_DIR}/lib/cmsis_device_f4)
set(CORE ${CMAKE_SOURCE_DIR}/lib/cmsis_core)
set(RTOS ${CMAKE_SOURCE_DIR}/lib/FreeRTOS-Kernel)
set(SRC ${CMAKE_SOURCE_DIR}/src)

set(LL_SOURCES
    ${LL}/Src/stm32f4xx_ll_adc.c
    ${LL}/Src/stm32f4xx_ll_dma.c
    ${LL}/Src/stm32f4xx_ll_exti.c
    ${LL}/Src/stm32f4xx_ll_gpio.c
    ${LL}/Src/stm32f4xx_ll_rcc.c
    ${LL}/Src/stm32f4xx_ll_tim.c
    ${LL}/Src/stm32f4xx_ll_usart.c
    ${LL}/Src/stm32f4xx_ll_utils.c
  )

file(GLOB_RECURSE APP_SOURCES CONFIGURE_DEPENDS
  ${CMAKE_SOURCE_DIR}/src/*.c)


add_executable(${PROJECT_NAME}.elf
  ${APP_SOURCES}
  ${LL_SOURCES}
  ${DEV}/Source/Templates/system_stm32f4xx.c
  ${DEV}/Source/Templates/gcc/startup_stm32f446xx.s
  ${RTOS}/tasks.c
  ${RTOS}/queue.c
  ${RTOS}/list.c
  ${RTOS}/timers.c
  ${RTOS}/event_groups.c
  ${RTOS}/stream_buffer.c
  ${RTOS}/portable/GCC/ARM_CM4F/port.c
  ${RTOS}/portable/MemMang/heap_4.c

)

target_include_directories(${PROJECT_NAME}.elf PRIVATE
  src ${CORE}/CMSIS/Core/Include ${DEV}/Include ${LL}/Inc
  ${RTOS}/include ${RTOS}/portable/GCC/ARM_CM4F)

target_compile_definitions(${PROJECT_NAME}.elf PRIVATE
  STM32F446xx USE_FULL_LL_DRIVER HSE_VALUE=8000000)

target_compile_options(${PROJECT_NAME}.elf PRIVATE
  ${CPU_FLAGS} -Og -g3 -Wall -Wextra
  -ffunction-sections -fdata-sections)

target_link_options(${PROJECT_NAME}.elf PRIVATE
  ${CPU_FLAGS}
  -T${CMAKE_SOURCE_DIR}/STM32F446xx_FLASH.ld
  -Wl,--gc-sections -Wl,-Map=${PROJECT_NAME}.map
  --specs=nano.specs --specs=nosys.specs)

add_custom_command(TARGET ${PROJECT_NAME}.elf POST_BUILD
  COMMAND ${CMAKE_SIZE} $<TARGET_FILE:${PROJECT_NAME}.elf>)

# `cmake --build build --target flash`
add_custom_target(flash
  COMMAND openocd -f interface/stlink.cfg -f target/stm32f4x.cfg
          -c "program $<TARGET_FILE:${PROJECT_NAME}.elf> verify reset exit"
  DEPENDS ${PROJECT_NAME}.elf)


option(LOG_ENABLE "Build the UART logger" ON)
if(LOG_ENABLE)
  target_compile_definitions(${PROJECT_NAME}.elf PRIVATE LOG_ENABLE LOG_LEVEL=4)
endif()

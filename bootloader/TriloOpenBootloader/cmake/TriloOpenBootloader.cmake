# Enable compile command to ease indexing with e.g. clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

# Compiler options
target_compile_options(${BUILD_UNIT_0_NAME} PRIVATE
    $<$<COMPILE_LANGUAGE:C>: ${CUBE_CMAKE_C_FLAGS}>
    $<$<COMPILE_LANGUAGE:CXX>: ${CUBE_CMAKE_CXX_FLAGS}>
    $<$<COMPILE_LANGUAGE:ASM>: ${CUBE_CMAKE_ASM_FLAGS}>
)

# Linker options
target_link_options(${BUILD_UNIT_0_NAME} PRIVATE ${CUBE_CMAKE_EXE_LINKER_FLAGS})

# Add sources to executable/library
target_sources(${BUILD_UNIT_0_NAME} PRIVATE
    # Core application files
    "Core/Src/main.c"
    "Core/Src/stm32h5xx_hal_msp.c"
    "Core/Src/stm32h5xx_hal_timebase_tim.c"
    "Core/Src/stm32h5xx_it.c"
    "Core/Src/system_stm32h5xx.c"
    "Application/Core/syscalls.c"
    "Application/Core/sysmem.c"
    "Application/Startup/startup_stm32h523cetx.s"
    
    # OpenBootloader application
    "OpenBootloader/App/app_openbootloader.c"
    
    # OpenBootloader interfaces (memory + communication via FDCAN/IWDG)
    "OpenBootloader/Target/common_interface.c"
    "OpenBootloader/Target/flash_interface.c"
    "OpenBootloader/Target/fdcan_interface.c"
    "OpenBootloader/Target/iwdg_interface.c"
    "OpenBootloader/Target/ram_interface.c"
    "OpenBootloader/Target/optionbytes_interface.c"
    "OpenBootloader/Target/otp_interface.c"
    "OpenBootloader/Target/engibytes_interface.c"
    "OpenBootloader/Target/systemmemory_interface.c"
    "Application/OpenBootloader/Target/uuid_interface.c"
    
    # HAL drivers (FDCAN, power, clock, core peripherals)
    "Drivers/STM32H5xx_HAL_Driver/Src/stm32h5xx_hal.c"
    "Drivers/STM32H5xx_HAL_Driver/Src/stm32h5xx_hal_cortex.c"
    "Drivers/STM32H5xx_HAL_Driver/Src/stm32h5xx_hal_dma.c"
    "Drivers/STM32H5xx_HAL_Driver/Src/stm32h5xx_hal_dma_ex.c"
    "Drivers/STM32H5xx_HAL_Driver/Src/stm32h5xx_hal_exti.c"
    "Drivers/STM32H5xx_HAL_Driver/Src/stm32h5xx_hal_fdcan.c"
    "Drivers/STM32H5xx_HAL_Driver/Src/stm32h5xx_hal_flash.c"
    "Drivers/STM32H5xx_HAL_Driver/Src/stm32h5xx_hal_flash_ex.c"
    "Drivers/STM32H5xx_HAL_Driver/Src/stm32h5xx_hal_gpio.c"
    "Drivers/STM32H5xx_HAL_Driver/Src/stm32h5xx_hal_icache.c"
    "Drivers/STM32H5xx_HAL_Driver/Src/stm32h5xx_hal_iwdg.c"
    "Drivers/STM32H5xx_HAL_Driver/Src/stm32h5xx_hal_pwr.c"
    "Drivers/STM32H5xx_HAL_Driver/Src/stm32h5xx_hal_pwr_ex.c"
    "Drivers/STM32H5xx_HAL_Driver/Src/stm32h5xx_hal_rcc.c"
    "Drivers/STM32H5xx_HAL_Driver/Src/stm32h5xx_hal_rcc_ex.c"
    "Drivers/STM32H5xx_HAL_Driver/Src/stm32h5xx_hal_tim.c"
    "Drivers/STM32H5xx_HAL_Driver/Src/stm32h5xx_hal_tim_ex.c"
    
    # Low-level drivers (LL)
    "Drivers/STM32H5xx_HAL_Driver/Src/stm32h5xx_ll_dma.c"
    "Drivers/STM32H5xx_HAL_Driver/Src/stm32h5xx_ll_exti.c"
    "Drivers/STM32H5xx_HAL_Driver/Src/stm32h5xx_ll_gpio.c"
    "Drivers/STM32H5xx_HAL_Driver/Src/stm32h5xx_ll_rcc.c"
    "Drivers/STM32H5xx_HAL_Driver/Src/stm32h5xx_ll_tim.c"
    "Drivers/STM32H5xx_HAL_Driver/Src/stm32h5xx_ll_utils.c"
    
    # OpenBootloader core and FDCAN protocol module
    "Middlewares/ST/OpenBootloader/Core/openbl_core.c"
    "Middlewares/ST/OpenBootloader/Modules/Mem/openbl_mem.c"
    "Middlewares/ST/OpenBootloader/Modules/FDCAN/openbl_fdcan_cmd.c"
)

target_include_directories(${BUILD_UNIT_0_NAME} PRIVATE
    "Core/Inc"
    "Drivers/STM32H5xx_HAL_Driver/Inc"
    "Drivers/STM32H5xx_HAL_Driver/Inc/Legacy"
    "Drivers/CMSIS/Device/ST/STM32H5xx/Include"
    "Drivers/CMSIS/Include"
    "OpenBootloader/App"
    "OpenBootloader/Target"
    "Middlewares/ST/OpenBootloader/Core"
    "Middlewares/ST/OpenBootloader/Modules/FDCAN"
    "Middlewares/ST/OpenBootloader/Modules/Mem"
)

configure_file("STM32H523CETx_FLASH.ld" "${CMAKE_BINARY_DIR}" COPYONLY)

set_target_properties(${BUILD_UNIT_0_NAME} PROPERTIES LINK_DEPENDS "STM32H523CETx_FLASH.ld")


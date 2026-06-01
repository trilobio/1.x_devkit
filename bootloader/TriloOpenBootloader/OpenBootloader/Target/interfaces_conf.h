/**
 ******************************************************************************
 * @file    interfaces_conf.h
 * @author  MCD Application Team
 * @brief   Contains Interfaces configuration
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2023 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 ******************************************************************************
 */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef INTERFACES_CONF_H
#define INTERFACES_CONF_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* Includes ------------------------------------------------------------------*/
#include "stm32h5xx_ll_gpio.h"
#include "stm32h5xx_ll_rcc.h"

#define MEMORIES_SUPPORTED          7U

/*-------------------------- Definitions for FDCAN ---------------------------*/
#define FDCANx                      FDCAN2
#define FDCANx_CLK_ENABLE()         __HAL_RCC_FDCAN_CLK_ENABLE()
#define FDCANx_CLK_DISABLE()        __HAL_RCC_FDCAN_CLK_DISABLE()
#define FDCANx_GPIO_CLK_TX_ENABLE() __HAL_RCC_GPIOB_CLK_ENABLE()
#define FDCANx_GPIO_CLK_RX_ENABLE() __HAL_RCC_GPIOB_CLK_ENABLE()

#define FDCANx_TX_TIMEOUT           100U // milliseconds
#define FDCANx_TX_PIN               GPIO_PIN_13
#define FDCANx_TX_GPIO_PORT         GPIOB
#define FDCANx_TX_AF                GPIO_AF9_FDCAN2
#define FDCANx_RX_PIN               GPIO_PIN_5
#define FDCANx_RX_GPIO_PORT         GPIOB
#define FDCANx_RX_AF                GPIO_AF9_FDCAN2
#if defined(BLD_J1_CONTROL) | defined(BLD_J2_CONTROL) | defined(BLD_J3_CONTROL)                    \
    | defined(BLD_J4_CONTROL) | defined(BLD_LED)
#define FDCANx_GPIO_CLK_STBY_ENABLE() __HAL_RCC_GPIOA_CLK_ENABLE()
#define FDCANx_STBY_PIN               GPIO_PIN_5
#define FDCANx_STBY_PORT              GPIOA
#elif defined(BLD_FTS)
#define FDCANx_GPIO_CLK_STBY_ENABLE() __HAL_RCC_GPIOA_CLK_ENABLE()
#define FDCANx_STBY_PIN               GPIO_PIN_3
#define FDCANx_STBY_PORT              GPIOA
#elif defined(BLD_TOOL_CONTROL) | defined(BLD_DECK_SLOT) | defined(BLD_INTERMODULARITY)
#define FDCANx_GPIO_CLK_STBY_ENABLE() __HAL_RCC_GPIOC_CLK_ENABLE()
#define FDCANx_STBY_PIN               GPIO_PIN_15
#define FDCANx_STBY_PORT              GPIOC
#endif

#define FDCANx_FORCE_RESET()   __HAL_RCC_FDCAN_CLK_DISABLE()
#define FDCANx_RELEASE_RESET() __HAL_RCC_FDCAN_CLK_DISABLE()

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* INTERFACES_CONF_H */

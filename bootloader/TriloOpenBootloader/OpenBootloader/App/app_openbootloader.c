/**
 ******************************************************************************
 * @file    app_openbootloader.c
 * @author  MCD Application Team
 * @brief   OpenBootloader application entry point
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

/* Includes ------------------------------------------------------------------*/
#include "main.h"

#include "openbl_core.h"
#include "openbl_mem.h"
#include "openbl_fdcan_cmd.h"

#include "app_openbootloader.h"
#include "fdcan_interface.h"
#include "flash_interface.h"
#include "ram_interface.h"
#include "optionbytes_interface.h"
#include "otp_interface.h"
#include "engibytes_interface.h"
#include "systemmemory_interface.h"
#include "iwdg_interface.h"

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
static OPENBL_HandleTypeDef FDCAN_Handle;
static OPENBL_HandleTypeDef IWDG_Handle;

static OPENBL_OpsTypeDef FDCAN_Ops = { OPENBL_FDCAN_Configuration,
		OPENBL_FDCAN_DeInit, OPENBL_FDCAN_ProtocolDetection,
		OPENBL_FDCAN_GetCommandOpcode, OPENBL_FDCAN_SendByte };

static OPENBL_OpsTypeDef IWDG_Ops = { OPENBL_IWDG_Configuration, NULL, NULL,
NULL, NULL };

/* Exported variables --------------------------------------------------------*/
uint16_t SpecialCmdList[SPECIAL_CMD_MAX_NUMBER] = {
SPECIAL_CMD_DEFAULT };

uint16_t ExtendedSpecialCmdList[EXTENDED_SPECIAL_CMD_MAX_NUMBER] = {
SPECIAL_CMD_DEFAULT };

OPENBL_CommandsTypeDef FDCAN_Cmd = { OPENBL_FDCAN_GetCommand,
		OPENBL_FDCAN_GetVersion, OPENBL_FDCAN_GetID, OPENBL_FDCAN_ReadMemory,
		OPENBL_FDCAN_WriteMemory, OPENBL_FDCAN_Go, NULL, NULL,
		OPENBL_FDCAN_EraseMemory, OPENBL_FDCAN_WriteProtect,
		OPENBL_FDCAN_WriteUnprotect, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
		OPENBL_FDCAN_SpecialCommand, OPENBL_FDCAN_ExtendedSpecialCommand };

/* Private function prototypes -----------------------------------------------*/
/* Private functions ---------------------------------------------------------*/

/**
 * @brief  Initialize open Bootloader.
 * @param  None.
 * @retval None.
 */
void OpenBootloader_Init(void) {
	/* Register FDCAN interfaces */
	FDCAN_Handle.p_Ops = &FDCAN_Ops;
	FDCAN_Handle.p_Cmd = OPENBL_FDCAN_GetCommandsList();
	OPENBL_FDCAN_SetCommandsList(&FDCAN_Cmd);

	OPENBL_RegisterInterface(&FDCAN_Handle);

	/* Register IWDG interfaces */
	IWDG_Handle.p_Ops = &IWDG_Ops;
	IWDG_Handle.p_Cmd = NULL;

	OPENBL_RegisterInterface(&IWDG_Handle);

	/* Initialize interfaces */
	OPENBL_Init();

	/* Initialize memories */
	OPENBL_MEM_RegisterMemory(&FLASH_Descriptor);
	OPENBL_MEM_RegisterMemory(&RAM_Descriptor);
	OPENBL_MEM_RegisterMemory(&OB_Descriptor);
	OPENBL_MEM_RegisterMemory(&OTP_Descriptor);
	OPENBL_MEM_RegisterMemory(&ICP_Descriptor);
	OPENBL_MEM_RegisterMemory(&EB_Descriptor);
	OPENBL_MEM_RegisterMemory(&UUID_Descriptor);
}

/**
 * @brief  DeInitialize open Bootloader.
 * @param  None.
 * @retval None.
 */
void OpenBootloader_DeInit(void) {
	System_DeInit();
}

/**
 * @brief  This function is used to select which protocol will be used when communicating with the host.
 * @param  None.
 * @retval None.
 */
void OpenBootloader_ProtocolDetection(void) {
	static uint32_t interface_detected = 0U;

	if (interface_detected == 0U) {
		interface_detected = OPENBL_InterfaceDetection();

		/* De-initialize the interfaces that are not detected */
		if (interface_detected == 1U) {
			OPENBL_InterfacesDeInit();
		}
	}

	if (interface_detected == 1U) {
		OPENBL_CommandProcess();
	}
}

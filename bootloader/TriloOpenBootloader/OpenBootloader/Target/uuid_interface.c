/*
 * uuid_interface.c
 *
 *  Created on: Jun 23, 2025
 *      Author: max
 */

/* Includes ------------------------------------------------------------------*/
#include "platform.h"
#include "common_interface.h"

#include "openbl_mem.h"

#include "app_openbootloader.h"
#include "uuid_interface.h"
#include "string.h"

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
/* Private function prototypes -----------------------------------------------*/
/* Exported variables --------------------------------------------------------*/
OPENBL_MemoryTypeDef UUID_Descriptor = { UUID_START_ADDRESS, UUID_END_ADDRESS,
UUID_SIZE, UUID_AREA, OPENBL_UUID_Read,
NULL,
NULL,
NULL,
NULL,
NULL,
NULL };

/* Exported functions --------------------------------------------------------*/

/**
 * @brief  This function is used to read data from a given address.
 * @param  Address The address to be read.
 * @retval Returns the read value.
 */
uint8_t OPENBL_UUID_Read(uint32_t Address) {
	uint32_t uid[3];

	uid[0] = HAL_GetUIDw0();
	uid[1] = HAL_GetUIDw1();
	uid[2] = HAL_GetUIDw2();
	uint8_t uid_8[12];
	// Copy in big-endian format, since we're trying to be consistent with the rest
	// of the bootloader
	for (int i = 0; i < 3; i++) {
		uid_8[i * 4 + 0] = (uid[i] >> 24) & 0xFF;
		uid_8[i * 4 + 1] = (uid[i] >> 16) & 0xFF;
		uid_8[i * 4 + 2] = (uid[i] >> 8) & 0xFF;
		uid_8[i * 4 + 3] = (uid[i] >> 0) & 0xFF;
	}
	return uid_8[Address - UID_BASE];
}

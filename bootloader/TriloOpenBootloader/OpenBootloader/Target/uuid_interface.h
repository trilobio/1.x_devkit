/*
 * uuid_interface.h
 *
 *  Created on: Jun 23, 2025
 *      Author: max
 */

#ifndef APPLICATION_OPENBOOTLOADER_TARGET_UUID_INTERFACE_H_
#define APPLICATION_OPENBOOTLOADER_TARGET_UUID_INTERFACE_H_

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* Includes ------------------------------------------------------------------*/
/* Exported types ------------------------------------------------------------*/
/* Exported constants --------------------------------------------------------*/
/* Exported macro ------------------------------------------------------------*/
/* Exported functions --------------------------------------------------------*/
uint8_t OPENBL_UUID_Read(uint32_t Address);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* APPLICATION_OPENBOOTLOADER_TARGET_UUID_INTERFACE_H_ */

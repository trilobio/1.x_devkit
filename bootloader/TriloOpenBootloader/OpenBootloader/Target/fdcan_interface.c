/**
 ******************************************************************************
 * @file    fdcan_interface.c
 * @author  MCD Application Team
 * @brief   Contains FDCAN HW configuration
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
#include "platform.h"
#include "interfaces_conf.h"

#include "openbl_core.h"
#include "openbl_fdcan_cmd.h"

#include "fdcan_interface.h"
#include "iwdg_interface.h"

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
static FDCAN_HandleTypeDef hfdcan;
static FDCAN_FilterTypeDef sFilterConfig;
static FDCAN_TxHeaderTypeDef TxHeader;
static FDCAN_RxHeaderTypeDef RxHeader;
static uint8_t FdcanDetected = 0U;
uint32_t FDCanDataLength = 0U;

/* Exported variables --------------------------------------------------------*/
uint8_t TxData[FDCAN_RAM_BUFFER_SIZE];
uint8_t RxData[FDCAN_RAM_BUFFER_SIZE];

/* Private function prototypes -----------------------------------------------*/
static void OPENBL_FDCAN_Init(void);

/* Private functions ---------------------------------------------------------*/

/**
 * @brief  This function is used to initialize the used FDCAN instance.
 * @retval None.
 */
static void OPENBL_FDCAN_Init(void) {
	/*                Bit time configuration:
	 Bit time parameter         | Nominal      |  Data
	 ---------------------------|--------------|----------------
	 fdcan_ker_ck               | 20 MHz       | 20 MHz
	 Time_quantum (tq)          | 50 ns        | 50 ns
	 Synchronization_segment    | 1 tq         | 1 tq
	 Propagation_segment        | 23 tq        | 1 tq
	 Phase_segment_1            | 8 tq         | 4 tq
	 Phase_segment_2            | 8 tq         | 4 tq
	 Synchronization_Jump_width | 8 tq         | 4 tq
	 Bit_length                 | 40 tq = 2 us | 10 tq = 0.5 us
	 Bit_rate                   | 0.5 MBit/s   | 2 MBit/s
	 */

	hfdcan.Instance = FDCANx;
	hfdcan.Init.ClockDivider = FDCAN_CLOCK_DIV1;
	hfdcan.Init.FrameFormat = FDCAN_FRAME_FD_BRS;
	hfdcan.Init.Mode = FDCAN_MODE_NORMAL;
	hfdcan.Init.AutoRetransmission = ENABLE;
	hfdcan.Init.TransmitPause = DISABLE;
	hfdcan.Init.ProtocolException = ENABLE;
#if defined(BLD_INTERMODULARITY) | defined(BLD_LED) | defined(BLD_DECK_SLOT)
	hfdcan.Init.NominalPrescaler = 1;
	hfdcan.Init.NominalSyncJumpWidth = 10;
	hfdcan.Init.NominalTimeSeg1 = 139;
	hfdcan.Init.NominalTimeSeg2 = 20;
	hfdcan.Init.DataPrescaler = 1;
	hfdcan.Init.DataSyncJumpWidth = 5;
	hfdcan.Init.DataTimeSeg1 = 29;
	hfdcan.Init.DataTimeSeg2 = 10;
	hfdcan.Init.StdFiltersNbr = 1;
	hfdcan.Init.ExtFiltersNbr = 1;
#else
	hfdcan.Init.NominalPrescaler = 4;
	hfdcan.Init.NominalSyncJumpWidth = 2;
	hfdcan.Init.NominalTimeSeg1 = 7;
	hfdcan.Init.NominalTimeSeg2 = 2;
	hfdcan.Init.DataPrescaler = 1;
	hfdcan.Init.DataSyncJumpWidth = 2;
	hfdcan.Init.DataTimeSeg1 = 7;
	hfdcan.Init.DataTimeSeg2 = 2;
	hfdcan.Init.StdFiltersNbr = 1;
	hfdcan.Init.ExtFiltersNbr = 1;
#endif
	hfdcan.Init.TxFifoQueueMode = FDCAN_TX_FIFO_OPERATION;

	if (HAL_FDCAN_Init(&hfdcan) != HAL_OK) {
		while (1)
			;
	}

	/* Configure Rx filter */
	sFilterConfig.IdType = FDCAN_STANDARD_ID;
	sFilterConfig.FilterIndex = 0U;
	sFilterConfig.FilterType = FDCAN_FILTER_RANGE;
	sFilterConfig.FilterConfig = FDCAN_FILTER_TO_RXFIFO0;
	sFilterConfig.FilterID1 = 0x00U;
	sFilterConfig.FilterID2 = 0xFFU; // Accept range from 0x0 to 0xFF
	if (HAL_FDCAN_ConfigFilter(&hfdcan, &sFilterConfig) != HAL_OK) {
		while (1)
			;
	}

	// Reject everything by default
	if (HAL_FDCAN_ConfigGlobalFilter(&hfdcan, FDCAN_REJECT, FDCAN_REJECT,
	FDCAN_FILTER_REMOTE, FDCAN_FILTER_REMOTE) != HAL_OK) {
		while (1)
			;
	}

	/* Prepare Tx Header */
	TxHeader.Identifier = 0x111U;
	TxHeader.IdType = FDCAN_STANDARD_ID;
	TxHeader.TxFrameType = FDCAN_DATA_FRAME;
	TxHeader.DataLength = FDCAN_DLC_BYTES_64;
	TxHeader.ErrorStateIndicator = FDCAN_ESI_ACTIVE;
	TxHeader.BitRateSwitch = FDCAN_BRS_ON;
	TxHeader.FDFormat = FDCAN_FD_CAN;
	TxHeader.TxEventFifoControl = FDCAN_NO_TX_EVENTS;
	TxHeader.MessageMarker = 0U;

	/* Configure the standby pin */
	GPIO_InitTypeDef GPIO_InitStruct = { 0 };
	/* GPIO Ports Clock Enable */
	FDCANx_GPIO_CLK_STBY_ENABLE();

	/*Configure GPIO pin Output Level */
	HAL_GPIO_WritePin(FDCANx_STBY_PORT, FDCANx_STBY_PIN, GPIO_PIN_RESET);

	/*Configure FDCAN STBY */
	GPIO_InitStruct.Pin = FDCANx_STBY_PIN;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(FDCANx_STBY_PORT, &GPIO_InitStruct);

	/* Start the FDCAN module */
	if (HAL_FDCAN_Start(&hfdcan) != HAL_OK) {
		while (1)
			;
	}
}

/* Exported functions --------------------------------------------------------*/

/**
 * @brief  This function is used to configure FDCAN pins and then initialize the used FDCAN instance.
 * @retval None.
 */
void OPENBL_FDCAN_Configuration(void) {
	OPENBL_FDCAN_Init();
}

/**
 * @brief  This function is used to De-initialize the FDCAN pins and instance.
 * @retval None.
 */
void OPENBL_FDCAN_DeInit(void) {
	/* Only de-initialize the FDCAN if it is not the current detected interface */
	if (FdcanDetected == 0U) {
		FDCANx_FORCE_RESET();
		FDCANx_RELEASE_RESET();

		HAL_GPIO_DeInit(FDCANx_TX_GPIO_PORT, FDCANx_TX_PIN);
		HAL_GPIO_DeInit(FDCANx_RX_GPIO_PORT, FDCANx_RX_PIN);
		HAL_GPIO_DeInit(FDCANx_STBY_PORT, FDCANx_STBY_PIN);
	}
}

/**
 * @brief  This function is used to detect if there is any activity on FDCAN protocol.
 * @retval Returns 1 if interface is detected else 0.
 */
uint8_t OPENBL_FDCAN_ProtocolDetection(void) {
	/* Check if FIFO 0 receive at least one message */
	if (HAL_FDCAN_GetRxFifoFillLevel(&hfdcan, FDCAN_RX_FIFO0) > 0U) {
		FdcanDetected = 1U;
	} else {
		FdcanDetected = 0U;
	}

	return FdcanDetected;
}

/**
 * @brief  This function is used to get the command opcode from the host.
 * @retval Returns the command.
 */
uint8_t OPENBL_FDCAN_GetCommandOpcode(void) {
	uint8_t command_opc = 0x0U;
	HAL_StatusTypeDef status = HAL_OK;

	/* Check if FIFO 0 receive at least one message */
	while (HAL_FDCAN_GetRxFifoFillLevel(&hfdcan, FDCAN_RX_FIFO0) < 1U) {
	}

	/* Retrieve Rx messages from RX FIFO0 */
	status = HAL_FDCAN_GetRxMessage(&hfdcan, FDCAN_RX_FIFO0, &RxHeader, RxData);
	FDCanDataLength = RxHeader.DataLength;
	/* Check for errors */
	if (status == HAL_ERROR) {
		command_opc = ERROR_COMMAND;
	} else {
		command_opc = RxHeader.Identifier;
	}

	return command_opc;
}

/**
 * @brief  This function is used to read one byte from FDCAN pipe.
 * @retval Returns the read byte.
 */
uint8_t OPENBL_FDCAN_ReadByte(void) {
	uint8_t byte;

	/* Check if FIFO 0 receive at least one message */
	while (HAL_FDCAN_GetRxFifoFillLevel(&hfdcan, FDCAN_RX_FIFO0) < 1U) {
		OPENBL_IWDG_Refresh();
	}

	/* Retrieve Rx messages from RX FIFO0 */
	HAL_FDCAN_GetRxMessage(&hfdcan, FDCAN_RX_FIFO0, &RxHeader, &byte);
	FDCanDataLength = RxHeader.DataLength;

	return byte;
}

/**
 * @brief  This function is used to read bytes from FDCAN pipe.
 * @param  pBuffer The data buffer where received data will be stored.
 * @retval None.
 */
void OPENBL_FDCAN_ReadBytes(uint8_t *pBuffer, uint32_t BufferSize) {
	/* Check if FIFO 0 receive at least one message */
	while (HAL_FDCAN_GetRxFifoFillLevel(&hfdcan, FDCAN_RX_FIFO0) < 1U) {
		OPENBL_IWDG_Refresh();
	}

	/* Retrieve Rx messages from RX FIFO0 */
	HAL_FDCAN_GetRxMessage(&hfdcan, FDCAN_RX_FIFO0, &RxHeader, pBuffer);
	FDCanDataLength = RxHeader.DataLength;
}

/**
 * @brief  This function is used to send one byte through FDCAN pipe.
 * @param  Byte The byte to be sent.
 * @retval None.
 */
void OPENBL_FDCAN_SendByte(uint8_t Byte) {
	TxHeader.DataLength = FDCAN_DLC_BYTES_1;

	while (HAL_FDCAN_GetTxFifoFreeLevel(&hfdcan) == 0U) {
	}

	HAL_FDCAN_AddMessageToTxFifoQ(&hfdcan, &TxHeader, &Byte);

	/* Wait that the data is completely sent (sent FIFO empty) or timeout is exceeded */
	uint32_t t0 = HAL_GetTick();
	while (__HAL_FDCAN_GET_FLAG(&hfdcan, FDCAN_FLAG_TX_FIFO_EMPTY)
			!= FDCAN_FLAG_TX_FIFO_EMPTY) {
		if ((uint32_t) (HAL_GetTick() - t0) >= FDCANx_TX_TIMEOUT)
			return;
	}

	/* Clear the complete flag */
	__HAL_FDCAN_CLEAR_FLAG(&hfdcan, FDCAN_FLAG_TX_FIFO_EMPTY);
}

/**
 * @brief  This function is used to send a buffer using FDCAN.
 * @param  pBuffer The data buffer to be sent.
 * @param  BufferSize The size of the data buffer to be sent.
 * @retval None.
 */
void OPENBL_FDCAN_SendBytes(uint8_t *pBuffer, uint32_t BufferSize) {
	TxHeader.DataLength = BufferSize;

	while (HAL_FDCAN_GetTxFifoFreeLevel(&hfdcan) == 0U) {
	}

	HAL_FDCAN_AddMessageToTxFifoQ(&hfdcan, &TxHeader, pBuffer);

	/* Wait that the data is completely sent (sent FIFO empty) or timeout is exceeded */
	uint32_t t0 = HAL_GetTick();
	while (__HAL_FDCAN_GET_FLAG(&hfdcan, FDCAN_FLAG_TX_FIFO_EMPTY)
			!= FDCAN_FLAG_TX_FIFO_EMPTY) {
		if ((uint32_t) (HAL_GetTick() - t0) >= FDCANx_TX_TIMEOUT)
			return;
	}

	/* Clear the complete flag */
	__HAL_FDCAN_CLEAR_FLAG(&hfdcan, FDCAN_FLAG_TX_FIFO_EMPTY);
}

/**
 * @brief  This function is used to process and execute the special commands.
 *         The user must define the special commands routine here.
 * @param  pSpecialCmd Pointer to special command structure.
 * @retval Returns NACK status in case of error else returns ACK status.
 */
void OPENBL_FDCAN_SpecialCommandProcess(OPENBL_SpecialCmdTypeDef *pSpecialCmd) {
	switch (pSpecialCmd->OpCode) {
	/* Unknown command opcode */
	default:
		if (pSpecialCmd->CmdType == OPENBL_SPECIAL_CMD) {
			/* Send NULL data size */
			TxData[0] = 0x0U;
			TxData[1] = 0x0U;

			/* Send NULL status size */
			TxData[2] = 0x0U;
			TxData[3] = 0x0U;

			OPENBL_FDCAN_SendBytes(TxData, FDCAN_DLC_BYTES_4);

			/* NOTE: In case of any operation that prevents the code from returning to Middleware (reset operation...),
			 to be compatible with the OpenBL protocol, the user must ensure sending the last ACK from here.
			 */
		} else if (pSpecialCmd->CmdType == OPENBL_EXTENDED_SPECIAL_CMD) {
			/* Send NULL status size */
			TxData[0] = 0x0U;
			TxData[1] = 0x0U;

			OPENBL_FDCAN_SendBytes(TxData, FDCAN_DLC_BYTES_2);

			/* NOTE: In case of any operation that prevents the code from returning to Middleware (reset operation...),
			 to be compatible with the OpenBL protocol, the user must ensure sending the last ACK from here.
			 */
		}

		break;
	}
}

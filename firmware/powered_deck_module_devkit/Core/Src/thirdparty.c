#include "thirdparty.h"

#include "fdcan.h"
#include "main.h"
#include "stm32h5xx_hal_fdcan.h"

void handleThirdPartyMessage(uint32_t id, const uint8_t* frame, size_t size) {
    (void)id;
    (void)frame;
    (void)size;
}

void HAL_FDCAN_RxFifo1Callback(FDCAN_HandleTypeDef* hfdcan, uint32_t RxFifo1ITs) {
    if ((RxFifo1ITs & FDCAN_IT_RX_FIFO1_NEW_MESSAGE) != RESET) {
        if (HAL_FDCAN_GetRxMessage(hfdcan, FDCAN_RX_FIFO1, &rx_header, rx_data) != HAL_OK) {
            Error_Handler();
        }

        handleThirdPartyMessage(rx_header.Identifier, rx_data, fdcanDataLengthToBytes(rx_header.DataLength));

        if (HAL_FDCAN_ActivateNotification(hfdcan, FDCAN_IT_RX_FIFO1_NEW_MESSAGE, 0) != HAL_OK) {
            Error_Handler();
        }
    }
}

#include "trilo.h"

#include "fdcan.h"
#include "main.h"
#include "stm32h5xx_hal_fdcan.h"

volatile bool receivedTriloMessage = false;
volatile uint32_t triloRxFifo0FullCount = 0;
volatile uint32_t triloRxFifo0LostCount = 0;

static TriloCanId parseTriloCanId(uint32_t id) {
    return (TriloCanId) {
        .priority = (uint8_t)((id & PRIORITY_MASK) >> PRIORITY_SHIFT),
        .position_id = (uint16_t)((id & POSITION_ID_MASK) >> POSITION_ID_SHIFT),
        .is_common = ((id & ISCOMMON_MASK) >> ISCOMMON_SHIFT) ? true : false,
        .message_type = (MessageType)((id & MSG_TYPE_MASK) >> MSG_TYPE_SHIFT),
        .command_id = (uint16_t)((id & COMMAND_ID_MASK) >> COMMAND_ID_SHIFT),
    };
}

static uint32_t trilo_can_id_to_uint32(TriloCanId can_id) {
    return ((uint32_t)(can_id.priority & WIDTH_MASK(PRIORITY_SZ)) << PRIORITY_SHIFT)
         | ((uint32_t)(can_id.position_id & WIDTH_MASK(POSITION_ID_SZ)) << POSITION_ID_SHIFT)
         | ((uint32_t)(can_id.is_common ? 1 : 0) << ISCOMMON_SHIFT)
         | ((uint32_t)(can_id.message_type & WIDTH_MASK(MSG_TYPE_SZ)) << MSG_TYPE_SHIFT)
         | ((uint32_t)(can_id.command_id & WIDTH_MASK(COMMAND_ID_SZ)) << COMMAND_ID_SHIFT);
}

static uint32_t getSmallestFDCANDlc(int size) {
    if (size <= 0) {
        return FDCAN_DLC_BYTES_0;
    } else if (size == 1) {
        return FDCAN_DLC_BYTES_1;
    } else if (size == 2) {
        return FDCAN_DLC_BYTES_2;
    } else if (size == 3) {
        return FDCAN_DLC_BYTES_3;
    } else if (size == 4) {
        return FDCAN_DLC_BYTES_4;
    } else if (size == 5) {
        return FDCAN_DLC_BYTES_5;
    } else if (size == 6) {
        return FDCAN_DLC_BYTES_6;
    } else if (size == 7) {
        return FDCAN_DLC_BYTES_7;
    } else if (size <= 8) {
        return FDCAN_DLC_BYTES_8;
    } else if (size <= 12) {
        return FDCAN_DLC_BYTES_12;
    } else if (size <= 16) {
        return FDCAN_DLC_BYTES_16;
    } else if (size <= 20) {
        return FDCAN_DLC_BYTES_20;
    } else if (size <= 24) {
        return FDCAN_DLC_BYTES_24;
    } else if (size <= 32) {
        return FDCAN_DLC_BYTES_32;
    } else if (size <= 48) {
        return FDCAN_DLC_BYTES_48;
    } else {
        return FDCAN_DLC_BYTES_64;
    }
}

static size_t fdcanDataLengthToBytes(uint32_t data_length) {
    switch (data_length) {
        case FDCAN_DLC_BYTES_0:
            return 0;
        case FDCAN_DLC_BYTES_1:
            return 1;
        case FDCAN_DLC_BYTES_2:
            return 2;
        case FDCAN_DLC_BYTES_3:
            return 3;
        case FDCAN_DLC_BYTES_4:
            return 4;
        case FDCAN_DLC_BYTES_5:
            return 5;
        case FDCAN_DLC_BYTES_6:
            return 6;
        case FDCAN_DLC_BYTES_7:
            return 7;
        case FDCAN_DLC_BYTES_8:
            return 8;
        case FDCAN_DLC_BYTES_12:
            return 12;
        case FDCAN_DLC_BYTES_16:
            return 16;
        case FDCAN_DLC_BYTES_20:
            return 20;
        case FDCAN_DLC_BYTES_24:
            return 24;
        case FDCAN_DLC_BYTES_32:
            return 32;
        case FDCAN_DLC_BYTES_48:
            return 48;
        case FDCAN_DLC_BYTES_64:
            return 64;
        default:
            return 0;
    }
}

static void configureFDCANFilters(FDCAN_HandleTypeDef* hfdcan, uint16_t position_id, uint16_t all_positions_id,
                                  void (*err_handler)(void)) {
    FDCAN_FilterTypeDef thisPositionMessage = {
        .IdType = FDCAN_EXTENDED_ID,
        .FilterIndex = 0,
        .FilterType = FDCAN_FILTER_MASK,
        .FilterConfig = FDCAN_FILTER_TO_RXFIFO0,
        .FilterID1 = PACK_POSITION_ID(position_id),
        .FilterID2 = POSITION_ID_MASK,
    };

    FDCAN_FilterTypeDef allPositionsMessage = {
        .IdType = FDCAN_EXTENDED_ID,
        .FilterIndex = 1,
        .FilterType = FDCAN_FILTER_MASK,
        .FilterConfig = FDCAN_FILTER_TO_RXFIFO0,
        .FilterID1 = PACK_POSITION_ID(all_positions_id),
        .FilterID2 = POSITION_ID_MASK,
    };

    if (HAL_FDCAN_ConfigFilter(hfdcan, &thisPositionMessage) != HAL_OK) {
        err_handler();
    }
    if (HAL_FDCAN_ConfigFilter(hfdcan, &allPositionsMessage) != HAL_OK) {
        err_handler();
    }
    if (HAL_FDCAN_ConfigGlobalFilter(hfdcan, FDCAN_REJECT, FDCAN_REJECT, FDCAN_REJECT_REMOTE,
                                     FDCAN_FILTER_REMOTE) != HAL_OK) {
        err_handler();
    }
}

static void startupFDCAN(FDCAN_HandleTypeDef* hfdcan, GPIO_TypeDef* port, uint16_t pin,
                         void (*err_handler)(void)) {
    if (HAL_FDCAN_GetState(hfdcan) != HAL_FDCAN_STATE_READY) {
        err_handler();
    }
    HAL_GPIO_WritePin(port, pin, GPIO_PIN_RESET);
    if (HAL_FDCAN_Start(hfdcan) != HAL_OK) {
        err_handler();
    }
    if (HAL_FDCAN_ActivateNotification(hfdcan,
                                       FDCAN_IT_RX_FIFO0_NEW_MESSAGE |
                                       FDCAN_IT_RX_FIFO0_FULL |
                                       FDCAN_IT_RX_FIFO0_MESSAGE_LOST,
                                       0) != HAL_OK) {
        err_handler();
    }
    if (HAL_FDCAN_ActivateNotification(hfdcan, FDCAN_IT_RX_FIFO1_NEW_MESSAGE, 0) != HAL_OK) {
        err_handler();
    }
    if (HAL_FDCAN_GetState(hfdcan) == HAL_FDCAN_STATE_ERROR) {
        err_handler();
    }
}

TriloSendStatus Trilo_SendTriloFrame(TriloFrame_t* frame) {
    uint8_t tx_data[64] = {0};
    TriloCanId can_id = {
        .priority = (frame->can_id.priority & ((1 << PRIORITY_SZ) - 1)),
        .position_id = (frame->can_id.position_id & ((1 << POSITION_ID_SZ) - 1)),
        .is_common = frame->can_id.is_common,
        .message_type = frame->can_id.message_type,
        .command_id = (frame->can_id.command_id & ((1 << COMMAND_ID_SZ) - 1)),
    };

    FDCAN_TxHeaderTypeDef tx_header = {
        .Identifier = trilo_can_id_to_uint32(can_id),
        .IdType = FDCAN_EXTENDED_ID,
        .TxFrameType = FDCAN_DATA_FRAME,
        .DataLength = getSmallestFDCANDlc(frame->data_length),
        .ErrorStateIndicator = FDCAN_ESI_ACTIVE,
        .BitRateSwitch = FDCAN_BRS_ON,
        .FDFormat = FDCAN_FD_CAN,
        .TxEventFifoControl = FDCAN_NO_TX_EVENTS,
        .MessageMarker = 0,
    };

    if (frame->data_length > sizeof(tx_data)) {
        return TRILO_SEND_ERROR;
    }

    memcpy(tx_data, frame->data, frame->data_length);
    HAL_StatusTypeDef status = HAL_FDCAN_AddMessageToTxFifoQ(&hfdcan2, &tx_header, tx_data);

    switch (status) {
        case HAL_OK:
            return TRILO_SEND_SUCCESS;
        case HAL_BUSY:
            return TRILO_SEND_QUEUE_FULL;
        default:
            return TRILO_SEND_ERROR;
    }
}

void readUID(uint8_t* buf) {
    uint32_t* uid = (uint32_t*)buf;
    uid[0] = HAL_GetUIDw0();
    uid[1] = HAL_GetUIDw1();
    uid[2] = HAL_GetUIDw2();
}

void triloCommsInit(FDCAN_HandleTypeDef* hfdcan, GPIO_TypeDef* port, uint16_t pin,
                    void (*err_handler)(void)) {
    configureFDCANFilters(hfdcan, POSITION_ID, ALL_POSITIONS_ID, err_handler);
    startupFDCAN(hfdcan, port, pin, err_handler);
}

void TRILO_COMMS_Init(void) {
    triloCommsInit(&hfdcan2, STBY_GPIO_Port, STBY_Pin, Error_Handler);
}

static void dispatchTriloFrame(const FDCAN_RxHeaderTypeDef* header, const uint8_t* data) {
    TriloFrame_t frame = {
        .can_id = parseTriloCanId(header->Identifier),
    };
    frame.data_length = fdcanDataLengthToBytes(header->DataLength);
    memcpy(frame.data, data, frame.data_length);

    if ((frame.can_id.message_type != PiRequest) && (frame.can_id.message_type != PiAlert)) {
        return;
    }

    if (frame.can_id.is_common) {
        handleTriloCommonMessage(&frame);
    } else {
        handleToolheadMessage(&frame);
    }
}

void handleTriloMessage(void) {
    do {
        receivedTriloMessage = false;

        while (HAL_FDCAN_GetRxFifoFillLevel(&hfdcan2, FDCAN_RX_FIFO0) > 0U) {
            FDCAN_RxHeaderTypeDef rx_header;
            uint8_t rx_data[64];

            if (HAL_FDCAN_GetRxMessage(&hfdcan2, FDCAN_RX_FIFO0, &rx_header, rx_data) != HAL_OK) {
                Error_Handler();
            }

            dispatchTriloFrame(&rx_header, rx_data);
        }
    } while (receivedTriloMessage);
}

void HAL_FDCAN_RxFifo0Callback(FDCAN_HandleTypeDef* hfdcan, uint32_t RxFifo0ITs) {
    (void)hfdcan;

    if ((RxFifo0ITs & FDCAN_IT_RX_FIFO0_FULL) != RESET) {
        triloRxFifo0FullCount++;
    }
    if ((RxFifo0ITs & FDCAN_IT_RX_FIFO0_MESSAGE_LOST) != RESET) {
        triloRxFifo0LostCount++;
    }
    if ((RxFifo0ITs & (FDCAN_IT_RX_FIFO0_NEW_MESSAGE |
                       FDCAN_IT_RX_FIFO0_FULL |
                       FDCAN_IT_RX_FIFO0_MESSAGE_LOST)) != RESET) {
        receivedTriloMessage = true;
    }
}

void JumpToBootloader(void) {
    NVIC_SystemReset();
    while (1) {
    }
}

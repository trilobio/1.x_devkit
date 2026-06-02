#include "protocol.h"

#include "fdcan.h"
#include "main.h"
#include "stm32h5xx_hal_fdcan.h"

#include <stddef.h>
#include <string.h>

volatile bool receivedCanMessage = false;

static void configureFDCANFilters(FDCAN_HandleTypeDef* hfdcan, uint16_t board_id, uint16_t all_call_id, void (*err_handler)(void));
static void startupFDCAN(FDCAN_HandleTypeDef* hfdcan, GPIO_TypeDef* port, uint16_t pin,
                         void (*err_handler)(void));
static uint8_t fdcanDlcToLength(uint32_t dlc);
static uint32_t lengthToFdcanDlc(uint8_t length);


static void CanInit(FDCAN_HandleTypeDef* hfdcan, GPIO_TypeDef* port, uint16_t pin,
                    void (*err_handler)(void)) {
    configureFDCANFilters(hfdcan, BOARD_ID, ALL_CALL_ID, err_handler);
    startupFDCAN(hfdcan, port, pin, err_handler);
}
static CanID parseCanID(uint32_t can_id) {
    CanID id;
    id.priority = (can_id >> (BOARD_ID_POSITION + BOARD_ID_SZ)) & ((1 << PRIORITY_SZ) - 1);
    id.board_id = (can_id >> BOARD_ID_POSITION) & ((1 << BOARD_ID_SZ) - 1);
    id.command_id = (can_id >> (REQUEST_ID_SZ + ERROR_FLAG_SZ)) & ((1 << COMMAND_ID_SZ) - 1);
    id.request_id = (can_id >> ERROR_FLAG_SZ) & ((1 << REQUEST_ID_SZ) - 1);
    id.error_flag = can_id & ((1 << ERROR_FLAG_SZ) - 1);
    return id;
}
static uint32_t constructCanID(CanID id) {
    return ((id.priority & ((1 << PRIORITY_SZ) - 1)) << (BOARD_ID_POSITION + BOARD_ID_SZ)) |
           ((id.board_id & ((1 << BOARD_ID_SZ) - 1)) << BOARD_ID_POSITION) |
           ((id.command_id & ((1 << COMMAND_ID_SZ) - 1)) << (REQUEST_ID_SZ + ERROR_FLAG_SZ)) |
           ((id.request_id & ((1 << REQUEST_ID_SZ) - 1)) << ERROR_FLAG_SZ) |
           (id.error_flag & ((1 << ERROR_FLAG_SZ) - 1));
}
static void processCanFrame(const FDCAN_RxHeaderTypeDef* rx_header, const uint8_t* rx_data);

CanFrame createCanFrame(CanID id, const uint8_t* data, uint8_t data_length) {
    CanFrame frame = {0};
    frame.id = id;

    if (data_length > sizeof(frame.data)) {
        data_length = sizeof(frame.data);
    }

    if (data != NULL && data_length > 0U) {
        memcpy(frame.data, data, data_length);
    }

    frame.data_length = data_length;
    return frame;
}

HAL_StatusTypeDef sendCanFrame(const CanFrame* frame) {
    if (frame == NULL) {
        return HAL_ERROR;
    }

    FDCAN_TxHeaderTypeDef tx_header = {0};
    tx_header.Identifier = constructCanID(frame->id);
    tx_header.IdType = FDCAN_EXTENDED_ID;
    tx_header.TxFrameType = FDCAN_DATA_FRAME;
    tx_header.DataLength = lengthToFdcanDlc(frame->data_length);
    tx_header.ErrorStateIndicator = FDCAN_ESI_ACTIVE;
    tx_header.BitRateSwitch = FDCAN_BRS_ON;
    tx_header.FDFormat = FDCAN_FD_CAN;
    tx_header.TxEventFifoControl = FDCAN_NO_TX_EVENTS;
    tx_header.MessageMarker = 0U;

    return HAL_FDCAN_AddMessageToTxFifoQ(&hfdcan2, &tx_header, (uint8_t*)frame->data);
}

void CanCommsInit(void) {
    CanInit(&hfdcan2, STBY_GPIO_Port, STBY_Pin, Error_Handler);
}


void handleCanMessage(void) {
    do {
        receivedCanMessage = false;

        while (HAL_FDCAN_GetRxFifoFillLevel(&hfdcan2, FDCAN_RX_FIFO0) > 0U) {
            FDCAN_RxHeaderTypeDef rx_header;
            uint8_t rx_data[64];

            if (HAL_FDCAN_GetRxMessage(&hfdcan2, FDCAN_RX_FIFO0, &rx_header, rx_data) != HAL_OK) {
                Error_Handler();
            }

            processCanFrame(&rx_header, rx_data);
        }
    } while (receivedCanMessage);
}

void HAL_FDCAN_RxFifo0Callback(FDCAN_HandleTypeDef* hfdcan, uint32_t RxFifo0ITs) {
    (void)hfdcan;

    if ((RxFifo0ITs & (FDCAN_IT_RX_FIFO0_NEW_MESSAGE |
                       FDCAN_IT_RX_FIFO0_FULL |
                       FDCAN_IT_RX_FIFO0_MESSAGE_LOST)) != RESET) {
        receivedCanMessage = true;
    }
}

static void configureFDCANFilters(FDCAN_HandleTypeDef* hfdcan, uint16_t board_id,
                          uint16_t CFG_ALL_BOARDS_ID, void (*err_handler)(void)) {
    FDCAN_FilterTypeDef thisBoardFilterConfig;
    thisBoardFilterConfig.IdType = FDCAN_EXTENDED_ID;
    thisBoardFilterConfig.FilterIndex = 0;
    thisBoardFilterConfig.FilterType = FDCAN_FILTER_MASK;
    thisBoardFilterConfig.FilterConfig = FDCAN_FILTER_TO_RXFIFO0;

    // ID_LENGTH << POSITION_OF_BOARD_ID_IN_CANID
    // 10 bits = 2^10 == 1024 - 1 = 0x3FF
    // Match on our board ID
    thisBoardFilterConfig.FilterID1 = (uint32_t)board_id << BOARD_ID_POSITION;
    // Mask to compare our board ID to the proper bits in the CANID, ignore the rest.
    thisBoardFilterConfig.FilterID2 = (uint32_t)(0x3FFu << BOARD_ID_POSITION);

    FDCAN_FilterTypeDef allBoardsFilterConfig;
    allBoardsFilterConfig.IdType = FDCAN_EXTENDED_ID;
    allBoardsFilterConfig.FilterIndex = 1;
    allBoardsFilterConfig.FilterType = FDCAN_FILTER_MASK;
    allBoardsFilterConfig.FilterConfig = FDCAN_FILTER_TO_RXFIFO0;

    allBoardsFilterConfig.FilterID1 = (uint32_t)CFG_ALL_BOARDS_ID << BOARD_ID_POSITION;
    allBoardsFilterConfig.FilterID2 = (uint32_t)(0x3FFu << BOARD_ID_POSITION);

    if (HAL_FDCAN_ConfigFilter(hfdcan, &thisBoardFilterConfig) != HAL_OK) {
        /* Filter configuration Error */
        err_handler();
    }

    if (HAL_FDCAN_ConfigFilter(hfdcan, &allBoardsFilterConfig) != HAL_OK) {
        /* Filter configuration Error */
        err_handler();
    }

    // Reject all standard ID data frames, reject non matching extended ID messages, reject standard remote frames, Filter extended remote frames
    if (HAL_FDCAN_ConfigGlobalFilter(hfdcan, FDCAN_REJECT, FDCAN_REJECT, FDCAN_REJECT_REMOTE,
                                     FDCAN_FILTER_REMOTE)
        != HAL_OK) {
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

static void processCanFrame(const FDCAN_RxHeaderTypeDef* rx_header, const uint8_t* rx_data) {
    if (rx_header->IdType != FDCAN_EXTENDED_ID || rx_header->RxFrameType != FDCAN_DATA_FRAME) {
        return;
    }

    CanID id = parseCanID(rx_header->Identifier);
    uint8_t data_length = fdcanDlcToLength(rx_header->DataLength);
    CanFrame response_frame = {0};
    bool should_send_response = false;
    bool response_error = false;

    switch (id.command_id) {
        case PING: {
            response_frame = createCanFrame(id, NULL, 0U);
            should_send_response = true;
            break;
        }
        case ADD_REQUEST: {
            if (data_length != sizeof(AddRequestData)) {
                break;
            }
            const AddRequestData* add_request_data = (const AddRequestData*)rx_data;
            AddResponseData add_response_data = {
                .a = (uint8_t)(add_request_data->a + add_request_data->b),
            };
            id.command_id = ADD_RESPONSE;
            response_frame = createCanFrame(id, (const uint8_t*)&add_response_data, sizeof(add_response_data));
            should_send_response = true;
            break;
        }
        case SUBTRACT_REQUEST: {
            if (data_length != sizeof(SubtractRequestData)) {
                break;
            }
            const SubtractRequestData* subtract_request_data = (const SubtractRequestData*)rx_data;
            SubtractResponseData subtract_response_data = {
                .a = (uint8_t)(subtract_request_data->a - subtract_request_data->b),
            };
            id.command_id = SUBTRACT_RESPONSE;
            response_frame = createCanFrame(id, (const uint8_t*)&subtract_response_data, sizeof(subtract_response_data));
            should_send_response = true;
            break;
        }
        case MULTIPLY_REQUEST: {
            if (data_length != sizeof(MultiplyRequestData)) {
                break;
            }
            const MultiplyRequestData* multiply_request_data = (const MultiplyRequestData*)rx_data;
            MultiplyResponseData multiply_response_data = {
                .a = (uint8_t)(multiply_request_data->a * multiply_request_data->b),
            };
            id.command_id = MULTIPLY_RESPONSE;
            response_frame = createCanFrame(id, (const uint8_t*)&multiply_response_data, sizeof(multiply_response_data));
            should_send_response = true;
            break;
        }
        case DIVIDE_REQUEST: {
            if (data_length != sizeof(DivideRequestData)) {
                break;
            }
            const DivideRequestData* divide_request_data = (const DivideRequestData*)rx_data;
            DivideResponseData divide_response_data = {0};
            if (divide_request_data->b == 0U) {
                response_error = true;
            } else {
                divide_response_data.a = (uint8_t)(divide_request_data->a / divide_request_data->b);
            }
            id.command_id = DIVIDE_RESPONSE;
            id.error_flag = response_error;
            response_frame = createCanFrame(id, (const uint8_t*)&divide_response_data, sizeof(divide_response_data));
            should_send_response = true;
            break;
        }
        case REBOOT: {
            NVIC_SystemReset();
            break;
        }
        default:
            // Handle unknown command or ignore
            break;
    }

    if (should_send_response && sendCanFrame(&response_frame) != HAL_OK) {
        Error_Handler();
    }
}

static uint8_t fdcanDlcToLength(uint32_t dlc) {
    switch (dlc) {
        case FDCAN_DLC_BYTES_0:
            return 0U;
        case FDCAN_DLC_BYTES_1:
            return 1U;
        case FDCAN_DLC_BYTES_2:
            return 2U;
        case FDCAN_DLC_BYTES_3:
            return 3U;
        case FDCAN_DLC_BYTES_4:
            return 4U;
        case FDCAN_DLC_BYTES_5:
            return 5U;
        case FDCAN_DLC_BYTES_6:
            return 6U;
        case FDCAN_DLC_BYTES_7:
            return 7U;
        case FDCAN_DLC_BYTES_8:
            return 8U;
        case FDCAN_DLC_BYTES_12:
            return 12U;
        case FDCAN_DLC_BYTES_16:
            return 16U;
        case FDCAN_DLC_BYTES_20:
            return 20U;
        case FDCAN_DLC_BYTES_24:
            return 24U;
        case FDCAN_DLC_BYTES_32:
            return 32U;
        case FDCAN_DLC_BYTES_48:
            return 48U;
        case FDCAN_DLC_BYTES_64:
            return 64U;
        default:
            return 0U;
    }
}

static uint32_t lengthToFdcanDlc(uint8_t length) {
    if (length <= 8U) {
        return length;
    }
    if (length <= 12U) {
        return FDCAN_DLC_BYTES_12;
    }
    if (length <= 16U) {
        return FDCAN_DLC_BYTES_16;
    }
    if (length <= 20U) {
        return FDCAN_DLC_BYTES_20;
    }
    if (length <= 24U) {
        return FDCAN_DLC_BYTES_24;
    }
    if (length <= 32U) {
        return FDCAN_DLC_BYTES_32;
    }
    if (length <= 48U) {
        return FDCAN_DLC_BYTES_48;
    }
    return FDCAN_DLC_BYTES_64;
}

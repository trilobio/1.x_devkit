#ifndef __PROTOCOL_H
#define __PROTOCOL_H
#ifdef __cplusplus
extern "C" {
#endif

#include "stm32h5xx_hal.h"
#include <stdbool.h>

#define BOARD_ID 0x102
#define ALL_CALL_ID 0x000
/*
*    Priority : Board ID : Command ID : Request ID : Error
*     2 bits      10 bits   8 bits       8 bits      1 bit
* 
*/

// Field widths (bits)
#define ERROR_FLAG_SZ     1
#define REQUEST_ID_SZ     8
#define COMMAND_ID_SZ     8
#define BOARD_ID_SZ       10
#define PRIORITY_SZ       2

// Bit budget (extended CAN ID is 29 bits) error checking
#define CANID_BITS        29
#define TOTAL_SZ          (ERROR_FLAG_SZ + REQUEST_ID_SZ + COMMAND_ID_SZ + BOARD_ID_SZ + PRIORITY_SZ)
#define BOARD_ID_POSITION (CANID_BITS - PRIORITY_SZ - BOARD_ID_SZ)

extern volatile bool receivedCanMessage; // True if CAN message received
extern uint8_t incoming_data[64]; // Buffer to store incoming data
extern bool processed_incoming_data; // False when incoming_data holds a fresh, unhandled message

typedef struct {
    uint8_t priority;
    uint16_t board_id;
    uint8_t command_id;
    uint8_t request_id;
    bool error_flag;
} CanID;
typedef struct {
    CanID id;
    uint8_t data[64];
    uint8_t data_length;
} CanFrame;

typedef enum {
    PING = 2,
    ARB_MSG_REQUEST = 63,
    ARB_MSG_RESPONSE = 65,
} CommandID;

void CanCommsInit(void);

CanFrame createCanFrame(CanID id, const uint8_t* data, uint8_t data_length);

HAL_StatusTypeDef sendCanFrame(const CanFrame* frame);

void handleCanMessage(void);

void HAL_FDCAN_RxFifo0Callback(FDCAN_HandleTypeDef* hfdcan, uint32_t RxFifo0ITs);

/*
────────────────────────────────────────────────┐
                                                │
*/
typedef struct __attribute__((packed)) {
    uint8_t data[64];
} ArbitraryMsgRequestData;

typedef struct __attribute__((packed)) {
    uint64_t a;
} ArbitraryMsgResponseData;

/*                                              │
────────────────────────────────────────────────┘
*/


#ifdef __cplusplus
}
#endif
#endif /* __PROTOCOL_H */

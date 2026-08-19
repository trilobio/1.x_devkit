#ifndef __PROTOCOL_H
#define __PROTOCOL_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

#include "stm32h5xx_hal.h"
#include <stdbool.h>

#define BOARD_ID 201
#define ALL_CALL_ID 0x000
/*
 *    Priority : Board ID : Command ID : Request ID : Error
 *     2 bits      10 bits   8 bits       8 bits      1 bit
 *
 */

// Field widths (bits)
#define ERROR_FLAG_SZ 1
#define REQUEST_ID_SZ 8
#define COMMAND_ID_SZ 8
#define BOARD_ID_SZ 10
#define PRIORITY_SZ 2

// Bit budget (extended CAN ID is 29 bits) error checking
#define CANID_BITS 29
#define TOTAL_SZ (ERROR_FLAG_SZ + REQUEST_ID_SZ + COMMAND_ID_SZ + BOARD_ID_SZ + PRIORITY_SZ)
#define BOARD_ID_POSITION (CANID_BITS - PRIORITY_SZ - BOARD_ID_SZ)

extern volatile bool receivedCanMessage; // True if CAN message received

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
    PING = 0x02,
    ADD_REQUEST = 0x12,
    ADD_RESPONSE = 0x13,
    SUBTRACT_REQUEST = 0x14,
    SUBTRACT_RESPONSE = 0x15,
    MULTIPLY_REQUEST = 0x16,
    MULTIPLY_RESPONSE = 0x17,
    DIVIDE_REQUEST = 0x18,
    DIVIDE_RESPONSE = 0x19,

    // 62 in decimal. also will be used to set pin mode instead (port, pin, mode,filler)
    SET_LIGHT_LEVELS = 0x3E,
    // Add more commands as needed
    REBOOT = 0xFF
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
    uint8_t a;
    uint8_t b;
} AddRequestData;

typedef struct __attribute__((packed)) {
    uint8_t a;
} AddResponseData;

/*                                              │
────────────────────────────────────────────────┘
*/

/*
────────────────────────────────────────────────┐
                                                │
*/
typedef struct __attribute__((packed)) {
    uint8_t a;
    uint8_t b;
} SubtractRequestData;

typedef struct __attribute__((packed)) {
    uint8_t a;
} SubtractResponseData;

/*                                              │
────────────────────────────────────────────────┘
*/

/*
────────────────────────────────────────────────┐
                                                │
*/
typedef struct __attribute__((packed)) {
    uint8_t a;
    uint8_t b;
} MultiplyRequestData;

typedef struct __attribute__((packed)) {
    uint8_t a;
} MultiplyResponseData;
/*                                              │
────────────────────────────────────────────────┘
*/

/*
────────────────────────────────────────────────┐
                                                │
*/
typedef struct __attribute__((packed)) {
    uint8_t a;
    uint8_t b;
} DivideRequestData;

typedef struct __attribute__((packed)) {
    uint8_t a;
} DivideResponseData;

/*                                              │
────────────────────────────────────────────────┘
*/

/*
────────────────────────────────────────────────┐
                                                │
*/
// Actuall set_light_levels fields but
typedef struct __attribute__((packed)) {
    uint8_t port;
    uint8_t pin;
    uint8_t mode;
    uint8_t _filler;
} SetPinModeRequestData;

typedef struct __attribute__((packed)) {
    uint8_t port;
    uint8_t pin;
    uint8_t mode;
    uint8_t _filler;
} SetPinModeResponseData;

/*                                              │
────────────────────────────────────────────────┘
*/

#ifdef __cplusplus
}
#endif
#endif /* __PROTOCOL_H */

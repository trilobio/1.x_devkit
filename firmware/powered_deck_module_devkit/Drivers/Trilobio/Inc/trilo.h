#ifndef __TRILO_H__
#define __TRILO_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "stm32h5xx_hal.h"
#include "common_messages.h"
#include "toolhead_messages.h"
#ifndef VERBOSE
#define VERBOSE 0
#endif

#ifndef SERIAL_NUMBER_BYTE_LENGTH
#define SERIAL_NUMBER_BYTE_LENGTH 8
#endif

#ifndef ALL_POSITIONS_ID
#define ALL_POSITIONS_ID 0
#endif

#ifndef POSITION_ID
#define POSITION_ID TOOL
#endif

#define UID_LENGTH 12

#define PING       2


/*
*    Priority : Position ID : IsCommon T/F :  Message Type : Command ID
*     2 bits      10 bits       1 bit         2 bits         14 bits
* 
*/
typedef enum {
    PiRequest = 0,
    NodeResponse = 1,
    PiAlert = 2,
    NodeAlert = 3,
} MessageType;

#define PRIORITY_SZ 2
#define POSITION_ID_SZ   10
#define ISCOMMON_SZ 1
#define MSG_TYPE_SZ 2
#define COMMAND_ID_SZ 14
#define TriloCanId_BITS    29
#define POSITION_ID_POSITION (TriloCanId_BITS - PRIORITY_SZ - POSITION_ID_SZ)

#define COMMAND_ID_SHIFT   0
#define MSG_TYPE_SHIFT     (COMMAND_ID_SHIFT + COMMAND_ID_SZ)  // 14
#define ISCOMMON_SHIFT     (MSG_TYPE_SHIFT + MSG_TYPE_SZ)      // 16
#define POSITION_ID_SHIFT  (ISCOMMON_SHIFT + ISCOMMON_SZ)      // 17
#define PRIORITY_SHIFT     (POSITION_ID_SHIFT + POSITION_ID_SZ)   // 27

#define WIDTH_MASK(width)       ((uint32_t)((width) >= 32 ? 0xFFFFFFFFu : ((1u << (width)) - 1u)))
#define MAKE_MASK(width, shift) ((uint32_t)(WIDTH_MASK(width) << (shift)))
#define COMMAND_ID_MASK         MAKE_MASK(COMMAND_ID_SZ, COMMAND_ID_SHIFT)
#define MSG_TYPE_MASK           MAKE_MASK(MSG_TYPE_SZ, MSG_TYPE_SHIFT)
#define ISCOMMON_MASK           MAKE_MASK(ISCOMMON_SZ, ISCOMMON_SHIFT)
#define POSITION_ID_MASK        MAKE_MASK(POSITION_ID_SZ, POSITION_ID_SHIFT)
#define PRIORITY_MASK           MAKE_MASK(PRIORITY_SZ, PRIORITY_SHIFT)
#define PACK_POSITION_ID(position_id) \
    (((uint32_t)(position_id) & WIDTH_MASK(POSITION_ID_SZ)) << POSITION_ID_SHIFT)

typedef struct  {
    uint8_t priority;
    uint16_t position_id;
    bool is_common;
    MessageType message_type;
    uint16_t command_id;
} TriloCanId;

typedef struct {
    TriloCanId can_id;
    uint8_t data[64];
    uint8_t data_length;
} TriloFrame_t;


enum PositionID {
    ALL_DEVICES = 0x000,
    
    TOOL    = 0x001,
    FTS     = 0x002,
    CAM_ARM = 0x003,
    
    ALL_J = 0x010,
    J1    = 0x011,
    J2    = 0x012,
    J3    = 0x013,
    J4    = 0x014,
    
    ALL_IM   = 0x020,
    IM_POS_X = 0x021,
    IM_POS_Y = 0x022,
    IM_NEG_X = 0x023,
    IM_NEG_Y = 0x024,
    
    ALL_LED         = 0x030,
    LED_POS_X_POS_Y = 0x031,
    LED_POS_X_NEG_Y = 0x032,
    LED_POS_Y_POS_X = 0x033,
    LED_POS_Y_NEG_X = 0x034,
    LED_NEG_X_POS_Y = 0x035,
    LED_NEG_X_NEG_Y = 0x036,
    LED_NEG_Y_POS_X = 0x037,
    LED_NEG_Y_NEG_X = 0x038,
    
    ALL_DECK_SLOT = 0x100,
    DECK_SLOT_1   = 0x101,
    DECK_SLOT_2   = 0x102,
    DECK_SLOT_3   = 0x103,
    DECK_SLOT_4   = 0x104,
    DECK_SLOT_5   = 0x105,
    DECK_SLOT_6   = 0x106,
    DECK_SLOT_7   = 0x107,
    DECK_SLOT_8   = 0x108,
    DECK_SLOT_9   = 0x109,
    DECK_SLOT_10  = 0x10A,
    DECK_SLOT_11  = 0x10B,
    DECK_SLOT_12  = 0x10C,
    DECK_SLOT_13  = 0x10D,
    DECK_SLOT_14  = 0x10E,
    DECK_SLOT_15  = 0x10F,
    DECK_SLOT_16  = 0x110,
    
    ALL_DECK_MODULE = 0x200,
    DECK_MODULE_1   = 0x201,
    DECK_MODULE_2   = 0x202,
    DECK_MODULE_3   = 0x203,
    DECK_MODULE_4   = 0x204,
    DECK_MODULE_5   = 0x205,
    DECK_MODULE_6   = 0x206,
    DECK_MODULE_7   = 0x207,
    DECK_MODULE_8   = 0x208,
    DECK_MODULE_9   = 0x209,
    DECK_MODULE_10  = 0x20A,
    DECK_MODULE_11  = 0x20B,
    DECK_MODULE_12  = 0x20C,
    DECK_MODULE_13  = 0x20D,
    DECK_MODULE_14  = 0x20E,
    DECK_MODULE_15  = 0x20F,
    DECK_MODULE_16  = 0x210
};

typedef enum {
    TRILO_SEND_SUCCESS,
    TRILO_SEND_ERROR,
    TRILO_SEND_QUEUE_FULL,
} TriloSendStatus;

typedef enum {
    TRILO_PARSE_SUCCESS,
    TRILO_PARSE_ERROR,
} TriloParseStatus;

typedef enum {
    TRILO_PACK_SUCCESS,
    TRILO_PACK_ERROR,
} TriloPackStatus;

#include "common_message_handlers.h"
#include "toolhead_message_handlers.h"

// TRILO COMMAND ID SPACE IS 0x000 - 0x3FF
// COMMON COMMANDS ARE FROM 0x000 - 0x0FF, POSITION-SPECIFIC COMMANDS ARE FROM 0x100 - 0x3FF


extern volatile bool receivedTriloMessage;
extern volatile uint32_t triloRxFifo0FullCount;
extern volatile uint32_t triloRxFifo0LostCount;

void triloCommsInit(FDCAN_HandleTypeDef* hfdcan, GPIO_TypeDef* port, uint16_t pin,
                     void (*err_handler)(void));

void TRILO_COMMS_Init(void);

TriloSendStatus Trilo_SendTriloFrame(TriloFrame_t* frame);

void handleTriloMessage(void);
void JumpToBootloader(void);

#ifdef __cplusplus
}
#endif

#endif

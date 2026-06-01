#include "common_message_handlers.h"
#include "common_command_handlers.h"
#include "common_messages.h"

static void handleTriloCommonRequestMessage(TriloCommonMessageID msg_id, const uint8_t* request_data, size_t request_data_length);
static void handleTriloCommonAlertMessage(TriloCommonMessageID msg_id, const uint8_t* alert_data, size_t alert_data_length);

void handleTriloCommonMessage(TriloFrame_t* frame) {
    if (!frame->can_id.is_common) {
        return;
    }

    if (frame->can_id.message_type == PiRequest) {
        handleTriloCommonRequestMessage((TriloCommonMessageID)frame->can_id.command_id, frame->data, frame->data_length);
    } else if (frame->can_id.message_type == PiAlert) {
        handleTriloCommonAlertMessage((TriloCommonMessageID)frame->can_id.command_id, frame->data, frame->data_length);
    } else {
        /* Nodes should not handle node-originated common messages, ignore */
        return;
    }
}

static void handleTriloCommonRequestMessage(TriloCommonMessageID msg_id, const uint8_t* request_data, size_t request_data_length) {
    (void)request_data;

    switch (msg_id) {
        case Ping:
            if (request_data_length != 0U) {
                break;
            }
            TriloCommon_HandlePing();
            break;
        case Version:
            if (request_data_length != 0U) {
                break;
            }
            TriloCommon_HandleVersion();
            break;
        default:
            /* Invalid command ID, ignore (not possible) */
            break;
    }
}

static void handleTriloCommonAlertMessage(TriloCommonMessageID msg_id, const uint8_t* alert_data, size_t alert_data_length) {
    (void)alert_data;
    (void)alert_data_length;

    switch (msg_id) {
        default:
            /* Invalid command ID, ignore (not possible) */
            break;
    }
}

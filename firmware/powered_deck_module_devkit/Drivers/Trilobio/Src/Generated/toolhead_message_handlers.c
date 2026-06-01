#include "toolhead_message_handlers.h"
#include "toolhead_command_handlers.h"
#include "toolhead_messages.h"
#include "trilo.h"

#include <stddef.h>
#include <string.h>

#define VAR_ARR_MAX_LENGTH 63U

static TriloFrame_t createToolheadResponseFrame(ToolheadResponseMessageID response_command_id,
                                                uint8_t* response_data,
                                                size_t response_data_length);
static TriloFrame_t createToolheadNodeAlertFrame(ToolheadNodeAlertMessageID alert_command_id, uint8_t* alert_data, size_t alert_data_length);
static void handleToolheadRequestMessage(ToolheadRequestMessageID msg_id, const uint8_t* request_data, size_t request_data_length);
static void handleToolheadPiAlertMessage(ToolheadPiAlertMessageID msg_id, const uint8_t* alert_data, size_t alert_data_length);

void sendToolheadNodeAlert(ToolheadNodeAlertMessageID alert_command_id, uint8_t* alert_data, size_t alert_data_length){
    TriloFrame_t alertFrame = createToolheadNodeAlertFrame(alert_command_id, alert_data, alert_data_length);
    if (Trilo_SendTriloFrame(&alertFrame) != TRILO_SEND_SUCCESS) {
        /* Handle send error */
    }
}
static void handleToolheadRequestMessage(ToolheadRequestMessageID msg_id, const uint8_t* request_data, size_t request_data_length){
    TriloFrame_t responseFrame;
    bool shouldSendResponse = false;

    switch (msg_id) {
        case AddCommandRequest: {
            if (request_data_length != sizeof(AddCommandRequestData)) {
                break;
            }
            const AddCommandRequestData* addRequestData = (const AddCommandRequestData*)request_data;
            AddCommandResponseData addResponseData = {0};

            if (Toolhead_HandleAddCommandRequest(addRequestData, &addResponseData)) {
                responseFrame = createToolheadResponseFrame(AddCommandResponse, (uint8_t*)&addResponseData,
                                                            sizeof(addResponseData));
                shouldSendResponse = true;
            }
            break;
        }

        case SubtractCommandRequest: {
            if (request_data_length != sizeof(SubtractCommandRequestData)) {
                break;
            }
            const SubtractCommandRequestData* subtractRequestData = (const SubtractCommandRequestData*)request_data;
            SubtractCommandResponseData subtractResponseData = {0};

            if (Toolhead_HandleSubtractCommandRequest(subtractRequestData, &subtractResponseData)) {
                responseFrame = createToolheadResponseFrame(SubtractCommandResponse,
                                                            (uint8_t*)&subtractResponseData,
                                                            sizeof(subtractResponseData));
                shouldSendResponse = true;
            }
            break;
        }

        case MultiplyCommandRequest: {
            if (request_data_length != sizeof(MultiplyCommandRequestData)) {
                break;
            }
            const MultiplyCommandRequestData* multiplyRequestData = (const MultiplyCommandRequestData*)request_data;
            MultiplyCommandResponseData multiplyResponseData = {0};

            if (Toolhead_HandleMultiplyCommandRequest(multiplyRequestData, &multiplyResponseData)) {
                responseFrame = createToolheadResponseFrame(MultiplyCommandResponse,
                                                            (uint8_t*)&multiplyResponseData,
                                                            sizeof(multiplyResponseData));
                shouldSendResponse = true;
            }
            break;
        }

        case DivideCommandRequest: {
            if (request_data_length != sizeof(DivideCommandRequestData)) {
                break;
            }
            const DivideCommandRequestData* divideRequestData = (const DivideCommandRequestData*)request_data;
            DivideCommandResponseData divideResponseData = {0};

            if (Toolhead_HandleDivideCommandRequest(divideRequestData, &divideResponseData)) {
                responseFrame = createToolheadResponseFrame(DivideCommandResponse,
                                                            (uint8_t*)&divideResponseData,
                                                            sizeof(divideResponseData));
                shouldSendResponse = true;
            }
            break;
        }

        case ReverseFixedLengthStringRequest: {
            if (request_data_length != sizeof(ReverseFixedLengthStringRequestData)) {
                break;
            }
            const ReverseFixedLengthStringRequestData* reverseStringRequestData = (const ReverseFixedLengthStringRequestData*)request_data;
            ReverseFixedLengthStringResponseData reverseStringResponseData = {0};

            if (Toolhead_HandleReverseFixedLengthStringRequest(reverseStringRequestData, &reverseStringResponseData)) {
                responseFrame = createToolheadResponseFrame(ReverseFixedLengthStringResponse,
                                                            (uint8_t*)&reverseStringResponseData,
                                                            sizeof(reverseStringResponseData));
                shouldSendResponse = true;
            }
            break;
        }

        case ReverseTwoFixedLengthStringsRequest: {
            if (request_data_length != sizeof(ReverseTwoFixedLengthStringsRequestData)) {
                break;
            }
            const ReverseTwoFixedLengthStringsRequestData* reverseTwoStringsRequestData = (const ReverseTwoFixedLengthStringsRequestData*)request_data;
            ReverseTwoFixedLengthStringsResponseData reverseTwoStringsResponseData = {0};

            if (Toolhead_HandleReverseTwoFixedLengthStringsRequest(reverseTwoStringsRequestData,
                                                                   &reverseTwoStringsResponseData)) {
                responseFrame = createToolheadResponseFrame(ReverseTwoFixedLengthStringsResponse,
                                                            (uint8_t*)&reverseTwoStringsResponseData,
                                                            sizeof(reverseTwoStringsResponseData));
                shouldSendResponse = true;
            }
            break;
        }

        case EchoUpTo63ByteStringRequest: {
            if (request_data_length < 1U) {
                break;
            }
            const EchoUpTo63ByteStringRequestData* echoRequestData = (const EchoUpTo63ByteStringRequestData*)request_data;

            if (echoRequestData->str_length > VAR_ARR_MAX_LENGTH) {
                break;
            }

            if ((size_t)echoRequestData->str_length + 1U > request_data_length) {
                break;
            }

            EchoUpTo63ByteStringResponseData echoResponseData = {0};

            if (Toolhead_HandleEchoUpTo63ByteStringRequest(echoRequestData, &echoResponseData)) {
                if (echoResponseData.str_length > VAR_ARR_MAX_LENGTH) {
                    echoResponseData.str_length = VAR_ARR_MAX_LENGTH;
                }
                responseFrame = createToolheadResponseFrame(EchoUpTo63ByteStringResponse,
                                                            (uint8_t*)&echoResponseData,
                                                            offsetof(EchoUpTo63ByteStringResponseData, str) +
                                                            echoResponseData.str_length);
                shouldSendResponse = true;
            }
            break;
        }
        default:
            /* Invalid command ID, ignore (not possible) */
            break;
    }

    if (shouldSendResponse && Trilo_SendTriloFrame(&responseFrame) != TRILO_SEND_SUCCESS) {
        /* Handle send error if necessary */
    }
}
static void handleToolheadPiAlertMessage(ToolheadPiAlertMessageID msg_id, const uint8_t* alert_data, size_t alert_data_length){
    switch (msg_id) {
    case JumpToBootloaderCommand: {
        if (alert_data_length != sizeof(JumpToBootloaderCommandData)) {
            break;
        }
        const JumpToBootloaderCommandData* alert = (const JumpToBootloaderCommandData*)alert_data;
        Toolhead_HandleJumpToBootloaderCommand(alert);
        break;
    }
    default:
        /* Invalid command ID, ignore (not possible) */
        break;
    }
}




void handleToolheadMessage(TriloFrame_t* frame) {
    if (frame->can_id.position_id != POSITION_ID) {
        return;
    }

    if (frame->can_id.message_type == PiAlert) {
        /* Handle PI alert messages */
        handleToolheadPiAlertMessage((ToolheadPiAlertMessageID)frame->can_id.command_id, frame->data, frame->data_length);
    } else if (frame->can_id.message_type == PiRequest) {
        /* Handle request messages */
        handleToolheadRequestMessage((ToolheadRequestMessageID)frame->can_id.command_id, frame->data, frame->data_length);
    } else {
        /* Toolhead should not receive node-originated messages, ignore */
        return;
    }
}


static TriloFrame_t createToolheadResponseFrame(ToolheadResponseMessageID response_command_id,
                                                uint8_t* response_data,
                                                size_t response_data_length) {
    TriloFrame_t response_frame = {0};
    response_frame.can_id.priority = 0;
    response_frame.can_id.position_id = POSITION_ID;
    response_frame.can_id.is_common = false;
    response_frame.can_id.message_type = NodeResponse;
    response_frame.can_id.command_id = response_command_id;
    if (response_data_length > sizeof(response_frame.data)) {
        response_data_length = sizeof(response_frame.data);
    }
    memcpy(response_frame.data, response_data, response_data_length);
    response_frame.data_length = (uint8_t)response_data_length;
    return response_frame;
}

static TriloFrame_t createToolheadNodeAlertFrame(ToolheadNodeAlertMessageID alert_command_id, uint8_t* alert_data, size_t alert_data_length) {
    TriloFrame_t alert_frame = {0};
    alert_frame.can_id.priority = 0;
    alert_frame.can_id.position_id = POSITION_ID;
    alert_frame.can_id.is_common = false;
    alert_frame.can_id.message_type = NodeAlert;
    alert_frame.can_id.command_id = alert_command_id;
    if (alert_data_length > sizeof(alert_frame.data)) {
        alert_data_length = sizeof(alert_frame.data);
    }
    memcpy(alert_frame.data, alert_data, alert_data_length);
    alert_frame.data_length = (uint8_t)alert_data_length;
    return alert_frame;
}

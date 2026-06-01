#include "toolhead_command_handlers.h"
#include "trilo.h"

#include <string.h>

bool Toolhead_HandleAddCommandRequest(const AddCommandRequestData* request,
                                      AddCommandResponseData* response) {
    response->a = request->a + request->b;
    return true;
}

bool Toolhead_HandleSubtractCommandRequest(const SubtractCommandRequestData* request,
                                           SubtractCommandResponseData* response) {
    response->a = request->a - request->b;
    return true;
}

bool Toolhead_HandleMultiplyCommandRequest(const MultiplyCommandRequestData* request,
                                           MultiplyCommandResponseData* response) {
    response->a = request->a * request->b;
    return true;
}

bool Toolhead_HandleDivideCommandRequest(const DivideCommandRequestData* request,
                                         DivideCommandResponseData* response) {
    response->a = (request->b == 0U) ? 0U : (uint8_t)(request->a / request->b);
    return true;
}

bool Toolhead_HandleReverseFixedLengthStringRequest(const ReverseFixedLengthStringRequestData* request,
                                                    ReverseFixedLengthStringResponseData* response) {
    for (size_t i = 0; i < sizeof(request->str); i++) {
        response->str[i] = request->str[sizeof(request->str) - 1U - i];
    }
    return true;
}

bool Toolhead_HandleReverseTwoFixedLengthStringsRequest(const ReverseTwoFixedLengthStringsRequestData* request,
                                                        ReverseTwoFixedLengthStringsResponseData* response) {
    for (size_t i = 0; i < sizeof(request->str); i++) {
        response->str[i] = request->str[sizeof(request->str) - 1U - i];
    }
    for (size_t i = 0; i < sizeof(request->str2); i++) {
        response->str2[i] = request->str2[sizeof(request->str2) - 1U - i];
    }
    return true;
}

bool Toolhead_HandleEchoUpTo63ByteStringRequest(const EchoUpTo63ByteStringRequestData* request,
                                                EchoUpTo63ByteStringResponseData* response) {
    response->str_length = request->str_length;
    memcpy(response->str, request->str, request->str_length);
    return true;
}

void Toolhead_HandleJumpToBootloaderCommand(const JumpToBootloaderCommandData* alert) {
    (void)alert;
    JumpToBootloader();
}

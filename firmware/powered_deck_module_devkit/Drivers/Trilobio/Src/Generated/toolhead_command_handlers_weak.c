#include "toolhead_command_handlers.h"

__attribute__((weak))
bool Toolhead_HandleAddCommandRequest(const AddCommandRequestData* request,
                                      AddCommandResponseData* response) {
    (void)request;
    (void)response;
    return false;
}

__attribute__((weak))
bool Toolhead_HandleSubtractCommandRequest(const SubtractCommandRequestData* request,
                                           SubtractCommandResponseData* response) {
    (void)request;
    (void)response;
    return false;
}

__attribute__((weak))
bool Toolhead_HandleMultiplyCommandRequest(const MultiplyCommandRequestData* request,
                                           MultiplyCommandResponseData* response) {
    (void)request;
    (void)response;
    return false;
}

__attribute__((weak))
bool Toolhead_HandleDivideCommandRequest(const DivideCommandRequestData* request,
                                         DivideCommandResponseData* response) {
    (void)request;
    (void)response;
    return false;
}

__attribute__((weak))
bool Toolhead_HandleReverseFixedLengthStringRequest(const ReverseFixedLengthStringRequestData* request,
                                                    ReverseFixedLengthStringResponseData* response) {
    (void)request;
    (void)response;
    return false;
}

__attribute__((weak))
bool Toolhead_HandleReverseTwoFixedLengthStringsRequest(const ReverseTwoFixedLengthStringsRequestData* request,
                                                        ReverseTwoFixedLengthStringsResponseData* response) {
    (void)request;
    (void)response;
    return false;
}

__attribute__((weak))
bool Toolhead_HandleEchoUpTo63ByteStringRequest(const EchoUpTo63ByteStringRequestData* request,
                                                EchoUpTo63ByteStringResponseData* response) {
    (void)request;
    (void)response;
    return false;
}

__attribute__((weak))
void Toolhead_HandleJumpToBootloaderCommand(const JumpToBootloaderCommandData* alert) {
    (void)alert;
}

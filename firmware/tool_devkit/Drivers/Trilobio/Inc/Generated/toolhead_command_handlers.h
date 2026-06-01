#ifndef TOOLHEAD_COMMAND_HANDLERS_H
#define TOOLHEAD_COMMAND_HANDLERS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "toolhead_messages.h"

bool Toolhead_HandleAddCommandRequest(const AddCommandRequestData* request,
                                      AddCommandResponseData* response);
bool Toolhead_HandleSubtractCommandRequest(const SubtractCommandRequestData* request,
                                           SubtractCommandResponseData* response);
bool Toolhead_HandleMultiplyCommandRequest(const MultiplyCommandRequestData* request,
                                           MultiplyCommandResponseData* response);
bool Toolhead_HandleDivideCommandRequest(const DivideCommandRequestData* request,
                                         DivideCommandResponseData* response);
bool Toolhead_HandleReverseFixedLengthStringRequest(const ReverseFixedLengthStringRequestData* request,
                                                    ReverseFixedLengthStringResponseData* response);
bool Toolhead_HandleReverseTwoFixedLengthStringsRequest(const ReverseTwoFixedLengthStringsRequestData* request,
                                                        ReverseTwoFixedLengthStringsResponseData* response);
bool Toolhead_HandleEchoUpTo63ByteStringRequest(const EchoUpTo63ByteStringRequestData* request,
                                                EchoUpTo63ByteStringResponseData* response);

void Toolhead_HandleJumpToBootloaderCommand(const JumpToBootloaderCommandData* alert);

#endif // TOOLHEAD_COMMAND_HANDLERS_H

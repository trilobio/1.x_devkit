#ifndef TOOLHEAD_MESSAGE_HANDLERS_H
#define TOOLHEAD_MESSAGE_HANDLERS_H
#include "toolhead_messages.h"
#include "trilo.h"

void sendToolheadNodeAlert(ToolheadNodeAlertMessageID alert_command_id, uint8_t* alert_data, size_t alert_data_length);
void handleToolheadMessage(TriloFrame_t* frame);

#endif // TOOLHEAD_MESSAGE_HANDLERS_H

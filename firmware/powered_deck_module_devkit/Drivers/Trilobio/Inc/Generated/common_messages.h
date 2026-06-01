#ifndef TRILO_COMMON_MESSAGES_H
#define TRILO_COMMON_MESSAGES_H
#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>

typedef enum {
    Ping = 0x000,
    Version = 0x001,
} TriloCommonMessageID;


#ifdef __cplusplus
}
#endif
#endif // TRILO_COMMON_MESSAGES_H

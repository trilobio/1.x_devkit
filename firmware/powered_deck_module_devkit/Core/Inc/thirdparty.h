#ifndef __THIRDPARTY_H__
#define __THIRDPARTY_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "stm32h5xx_hal.h"


void handleThirdPartyMessage(uint32_t id, const uint8_t* frame, size_t size);



#ifdef __cplusplus
}
#endif

#endif // __THIRDPARTY_H__
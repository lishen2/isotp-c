#include "isotp_user.h"
#include "isotp_defines.h"
#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>

// 调试信息回调
void isotp_user_debug(const char* format, ...) {
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    printf("\n");
}

// 模拟CAN发送函数
int isotp_user_send_can(const uint32_t arbitration_id, 
                       const uint8_t* data, const uint8_t size) {
    printf("Send CAN ID: 0x%X, Data:", arbitration_id);
    for(int i=0; i<size; i++) {
        printf(" %02X", data[i]);
    }
    printf("\n");
    return ISOTP_RET_OK;
}

// 模拟获取时间函数
uint32_t isotp_user_get_ms(void) {
    static uint32_t counter = 0;
    return counter += 10; // 模拟10ms递增
}

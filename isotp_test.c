#include "isotp.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdarg.h>

// 调试信息回调
// 函数用于调试，输出格式化的字符串
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

int main() {
    IsoTpLink link;
    uint8_t send_buf[512];
    uint8_t recv_buf[512];
    
    // 初始化ISO-TP链接
    isotp_init_link(&link, 0x7E0, send_buf, sizeof(send_buf), recv_buf, sizeof(recv_buf));
    
    // 测试单帧发送
    uint8_t single_frame_data[] = {0x01, 0x02, 0x03};
    printf("Testing single frame send:\n");
    int ret = isotp_send(&link, single_frame_data, sizeof(single_frame_data));
    printf("Send result: %d\n", ret);
    
    // 测试多帧发送
    uint8_t multi_frame_data[20];
    memset(multi_frame_data, 0xAA, sizeof(multi_frame_data));
    printf("\nTesting multi-frame send:\n");
    ret = isotp_send(&link, multi_frame_data, sizeof(multi_frame_data));
    printf("Send result: %d\n", ret);
    
    // 模拟轮询处理
    printf("\nPolling...\n");
    for(int i=0; i<5; i++) {
        isotp_poll(&link);
    }
    
    return 0;
}

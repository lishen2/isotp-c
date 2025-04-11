#include "isotp.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdarg.h>
#include <thread>
#include <mutex>
#include <vector>
#include <chrono>

#define SENDER_CAN_ID 0x7E0
#define RECEIVER_CAN_ID 0x7E1
#define BUFFER_SIZE 512

// 线程同步控制
std::mutex sender_mutex;
std::mutex receiver_mutex;
std::condition_variable cv;
std::atomic<bool> running{true}; // 线程运行控制标志

// 调试信息回调
void isotp_user_debug(const char* format, ...) {
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    printf("\n");
}

// 全局链接指针用于消息转发
IsoTpLink* g_receiver_link = nullptr;
// 全局链接指针用于消息转发
IsoTpLink* g_sender_link = nullptr;

// 模拟CAN发送函数
int isotp_user_send_can(const uint32_t arbitration_id, 
                       const uint8_t* data, const uint8_t size) {
   

    // 将消息转发到接收端
    if(g_receiver_link && arbitration_id == RECEIVER_CAN_ID) {
        printf("SendertoReceiver CAN ID: 0x%X, Data:", arbitration_id);
        for(int i=0; i<size; i++) {
            printf(" %02X", data[i]);
        }
        printf("\n");
        printf("Forwarding message to receiver link\n");
        // 移除const限定符以匹配函数签名
        isotp_on_can_message(g_receiver_link, (uint8_t*)data, size);
    }else if(g_sender_link && arbitration_id == SENDER_CAN_ID) {
        printf("ReceiverToSender CAN ID: 0x%X, Data:", arbitration_id);
        for(int i=0; i<size; i++) {
            printf(" %02X", data[i]);
        }
        printf("\n");
        printf("Forwarding message to sender link\n");
        // 移除const限定符以匹配函数签名
        isotp_on_can_message(g_sender_link, (uint8_t*)data, size);
    } else {
        printf("Unknown CAN ID: 0x%X\n", arbitration_id);
        
    }
    
    return ISOTP_RET_OK;
}

// 模拟获取时间函数
uint32_t isotp_user_get_ms(void) {
    static uint32_t counter = 0;
    return counter += 10; // 模拟10ms递增
}

// 模拟流控帧发送
void send_flow_control(IsoTpLink* link, uint8_t fs, uint8_t bs, uint8_t stmin) {
    uint8_t fc_frame[3] = {0x30 | fs, bs, stmin};
    printf("Sending Flow Control Frame to CAN ID 0x%X: ", link->send_arbitration_id);
    for(int i=0; i<3; i++) {
        printf("%02X ", fc_frame[i]);
    }
    printf("\n");
    isotp_on_can_message(link, fc_frame, sizeof(fc_frame));
    printf("Sent Flow Control: FS=0x%X, BS=%d, STmin=%d\n", fs, bs, stmin);
}

// UDS诊断响应处理
void handle_uds_request(const uint8_t* data, uint16_t size, IsoTpLink* link) {
    if(size < 1) return;
    
    uint8_t sid = data[0];
    std::vector<uint8_t> response;
    
    switch(sid) {
        case 0x22: // ReadDataByIdentifier
            if(size >= 3 && data[1] == 0xF1 && data[2] == 0x90) {
                response = {0x62, 0xF1, 0x90, 0x01, 0x02, 0x03, 0x04};
            }
            break;
        case 0x3E: // TesterPresent
            response = {0x7E};
            break;
        default:
            response = {0x7F, sid, 0x11}; // ServiceNotSupported
    }
    
    if(!response.empty()) {
        isotp_send(link, response.data(), response.size());
    }
}

// 模拟接收处理
void handle_received_data(IsoTpLink* link) {
    uint8_t payload[BUFFER_SIZE];
    uint16_t out_size;
    int ret = isotp_receive(link, payload, sizeof(payload), &out_size);
    switch(ret) {
        case ISOTP_RET_OK:
            printf("Received %d bytes: ", out_size);
            for(int i=0; i<out_size; i++) {
                printf("%02X ", payload[i]);
            }
            printf("\n");           
            // 处理UDS请求
            handle_uds_request(payload, out_size, link);
            break;
        case ISOTP_RET_NO_DATA:
            break;
        default:
            printf("Receive error: %d\n", ret);
    }
}

void sender_thread(IsoTpLink* link) {
    printf("Sender thread started\n");
    try {
        // UDS诊断请求及其长度
        const struct {
            uint8_t data[8];
            uint8_t size;
        } uds_requests[] = {
            {{0x3E, 0x00}, 2}, // TesterPresent
            {{0x22, 0xF1, 0x90}, 3}, // ReadDataByIdentifier
            {{0x10, 0x01}, 2} // DiagnosticSessionControl
        };
        
        for(const auto& req : uds_requests) {
            printf("Preparing to send UDS request: SID 0x%02X (length: %d)\n", req.data[0], req.size);
            std::this_thread::sleep_for(std::chrono::milliseconds(50)); // 增加初始延迟
            
            int ret = ISOTP_RET_ERROR;
            for(int attempt = 0; attempt < 3; ++attempt) { // 最多尝试3次
                {
                    printf("Attempt%d: Acquired lock for sending\n",attempt);
                    ret = isotp_send(link, req.data, req.size);
                    if(ret == ISOTP_RET_OK) break;
                    printf("Send failed, retrying...\n");
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            
            printf("Sent UDS request: SID 0x%02X, final result: %d\n", req.data[0], ret);
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    } catch(const std::exception& e) {
        printf("Exception in sender thread: %s\n", e.what());
    }
    printf("Sender thread finished\n");
    while (1)
    {
        /* code */
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
}

void receiver_thread(IsoTpLink* link) {
    printf("Receiver thread started\n");
    try {
        // 持续处理接收到的消息
        while(running) {
            {
                std::lock_guard<std::mutex> lock(receiver_mutex);
                isotp_poll(link);
                handle_received_data(link);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    } catch(const std::exception& e) {
        printf("Exception in receiver thread: %s\n", e.what());
    }
    printf("Receiver thread finished\n");
}

int main() {
    printf("Initializing ISO-TP links...\n");
    
    printf("Initializing sender link (CAN ID: 0x%X -> 0x%X)\n", SENDER_CAN_ID, RECEIVER_CAN_ID);
    IsoTpLink sender_link;
    g_sender_link = &sender_link; // 全局变量指向发送链接
    uint8_t sender_send_buf[BUFFER_SIZE], sender_recv_buf[BUFFER_SIZE];
    memset(&sender_link, 0, sizeof(sender_link));
    isotp_init_link(&sender_link, SENDER_CAN_ID, 
                  sender_send_buf, sizeof(sender_send_buf),
                  sender_recv_buf, sizeof(sender_recv_buf));
    sender_link.send_arbitration_id = RECEIVER_CAN_ID; // 明确设置目标CAN ID
    sender_link.send_status = ISOTP_SEND_STATUS_IDLE;

    printf("Initializing receiver link (CAN ID: 0x%X -> 0x%X)\n", RECEIVER_CAN_ID, SENDER_CAN_ID);
    static IsoTpLink receiver_link;
    g_receiver_link = &receiver_link;
    uint8_t receiver_send_buf[BUFFER_SIZE], receiver_recv_buf[BUFFER_SIZE];
    memset(&receiver_link, 0, sizeof(receiver_link));
    isotp_init_link(&receiver_link, RECEIVER_CAN_ID,
                  receiver_send_buf, sizeof(receiver_send_buf),
                  receiver_recv_buf, sizeof(receiver_recv_buf));
    receiver_link.send_arbitration_id = SENDER_CAN_ID; // 明确设置目标CAN ID

    printf("Creating sender thread...\n");
    // 创建发送线程
    std::thread sender(sender_thread, &sender_link);

    // 创建接收线程
    std::thread receiver(receiver_thread, &receiver_link);

    printf("Waiting for sender thread to finish...\n");
    // 等待发送线程完成
    sender.join();
    
    // 通知接收线程退出
    running = false;
    printf("Waiting for receiver thread to finish...\n");
    receiver.join();
    
    // 确保所有锁已释放
    {
        std::lock_guard<std::mutex> lock1(sender_mutex);
        std::lock_guard<std::mutex> lock2(receiver_mutex);
    }
    printf("All threads finished\n");
    return 0;
}

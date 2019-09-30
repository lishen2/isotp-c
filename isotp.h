#ifndef __ISOTP_H__
#define __ISOTP_H__

#include <stdio.h>
#include <string.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#include "isotp_defines.h"
#include "isotp_config.h"

/**
 * @brief  Callback function to send data to link layer.
 *
 * @param  [in] _id   Message id information.
 * @param  [in] _buf  Send buffer.
 * @param  [in] _size Send size.
 * @return Sent size.
 */
typedef uint8_t (*send_callback_t)(const uint32_t _id, const uint8_t *const _buf, const uint8_t _size);

/**
 * @brief  Callback function to receive data from link layer.
 * @param  [in] _id   Message id information.
 * @param  [in] _buf  Receive buffer.
 * @param  [in] _size Receive size.
 * @return Received size.
 */
typedef uint8_t (*receive_callback_t)(uint32_t *const _id, uint8_t *const _buf, const uint8_t _size);

/**
 * @brief  Callback function to get current system time in milliseconds.
 *
 * @return Current time in ms.
 */
typedef uint32_t (*sys_time_ms_callback_t)(void);

/**
 * @brief Callback function for debug.
 *
 * @param [in] _message Formated debug message to print.
 */
typedef void (*debug_callback_t)(const char* _message, ...);

/**
 * @brief Struct containing the data for linking an application to a CAN instance.
 * The data stored in this struct is used internally and may be used by software programs
 * using this library.
 */
typedef struct IsoTpLink {
    /* sender paramters */
    uint32_t                    send_arbitration_id; /* used to reply consecutive frame */
    /* message buffer */
    uint8_t*                    send_buffer;
    uint16_t                    send_buf_size;
    uint16_t                    send_size;
    uint16_t                    send_offset;
    /* multi-frame flags */
    uint8_t                     send_sn;
    uint16_t                    send_bs_remain; /* Remaining block size */
    uint8_t                     send_st_min;    /* Separation Time between consecutive frames, unit millis */
    uint8_t                     send_wtf_count; /* Maximum number of FC.Wait frame transmissions  */
    uint32_t                    send_timer_st;  /* Last time send consecutive frame */    
    uint32_t                    send_timer_bs;  /* Time until reception of the next FlowControl N_PDU
                                                   start at sending FF, CF, receive FC
                                                   end at receive FC */
    int                         send_protocol_result;
    uint8_t                     send_status;

    /* receiver paramters */
    uint32_t                    receive_arbitration_id;
    /* message buffer */
    uint8_t*                    receive_buffer;
    uint16_t                    receive_buf_size;
    uint16_t                    receive_size;
    uint16_t                    receive_offset;
    /* multi-frame control */
    uint8_t                     receive_sn;
    uint8_t                     receive_bs_count; /* Maximum number of FC.Wait frame transmissions  */
    uint32_t                    receive_timer_cr; /* Time until transmission of the next ConsecutiveFrame N_PDU
                                                     start at sending FC, receive CF 
                                                     end at receive FC */
    int                         receive_protocol_result;
    uint8_t                     receive_status;                                                     
	send_callback_t             send_callback;
	receive_callback_t          receive_callback;
	void (*indication_callback) (struct IsoTpLink *const _link, const uint32_t _id, const uint8_t *const _buf, const uint16_t _size);
	sys_time_ms_callback_t      sys_time_ms_callback;
	debug_callback_t            debug_callback;
} IsoTpLink;

/**
 * @brief Callback function to indicate data of network layer ready.
 *
 * @param [in] _link ISOTP link.
 * @param [in] _id   Message id information.
 * @param [in] _buf  Indication buffer.
 * @param [in] _size Indication size.
 */
typedef void (*indication_callback_t)(IsoTpLink *const _link, const uint32_t _id, const uint8_t *const _buf, const uint16_t _size);

/**
 * @brief Initialises the ISO-TP library.
 *
 * @param link The @code IsoTpLink @endcode instance used for transceiving data.
 * @param sendid The ID used to send data to other CAN nodes.
 * @param recvid The ID used to receive data from other CAN nodes.
 * @param sendbuf A pointer to an area in memory which can be used as a buffer for data to be sent.
 * @param sendbufsize The size of the buffer area.
 * @param recvbuf A pointer to an area in memory which can be used as a buffer for data to be received.
 * @param recvbufsize The size of the buffer area.
 * @param send_callback Called to send data to CAN.
 * @param receive_callback Called when need receive data from CAN.
 * @param indication_callback Called to indicate application layer after receiving full data.
 * @param sys_time_ms_callback Get system time in milliseconds.
 * @param debug_callback Print debug info.
 */
void isotp_init_link(IsoTpLink *link, uint32_t sendid, uint32_t recvid,
                     uint8_t *sendbuf, uint16_t sendbufsize,
                     uint8_t *recvbuf, uint16_t recvbufsize,	
	                 send_callback_t send_callback,
				     receive_callback_t receive_callback,
				     indication_callback_t indication_callback,
				     sys_time_ms_callback_t sys_time_ms_callback,
					 debug_callback_t debug_callback);

/**
 * @brief Polling function; call this function periodically to handle timeouts, send consecutive frames, etc.
 *
 * @param link The @code IsoTpLink @endcode instance used.
 */
void isotp_poll(IsoTpLink *link);

/**
 * @brief Handles incoming CAN messages.
 * Determines whether an incoming message is a valid ISO-TP frame or not and handles it accordingly.
 *
 * @param link The @code IsoTpLink @endcode instance used for transceiving data.
 * @param _id  Message id.
 * @param data The data received via CAN.
 * @param len The length of the data received.
 */
void isotp_indication(IsoTpLink *link, const uint32_t _id, const uint8_t *const data, const uint8_t len);

/**
 * @brief Send network layer message.
 *
 * @param [in] _link ISOTP link.
 * @param [in] _buf  Send buffer.
 * @param [in] _size Send size.
 */
int isotp_send_poll(IsoTpLink *const _link, const uint8_t *const _buf, const uint16_t _size);

/**
 * @brief Sends ISO-TP frames via CAN, using the ID set in the initialising function.
 *
 * Single-frame messages will be sent immediately when calling this function.
 * Multi-frame messages will be sent consecutively when calling isotp_poll.
 *
 * @param link The @code IsoTpLink @endcode instance used for transceiving data.
 * @param payload The payload to be sent. (Up to 4095 bytes).
 * @param size The size of the payload to be sent.
 *
 * @return Possible return values:
 *  - @code ISOTP_RET_OVERFLOW @endcode
 *  - @code ISOTP_RET_INPROGRESS @endcode
 *  - @code ISOTP_RET_OK @endcode
 *  - The return value of the user shim function isotp_user_send_can().
 */
int isotp_send(IsoTpLink *link, const uint8_t payload[], uint16_t size);

/**
 * @brief Receives and parses the received data and copies the parsed data in to the internal buffer.
 * @param link The @link IsoTpLink @endlink instance used to transceive data.
 * @param payload A pointer to an area in memory where the raw data is copied from.
 * @param payload_size The size of the received (raw) CAN data.
 * @param out_size A reference to a variable which will contain the size of the actual (parsed) data.
 *
 * @return Possible return values:
 *      - @link ISOTP_RET_OK @endlink
 *      - @link ISOTP_RET_NO_DATA @endlink
 */
int isotp_receive(IsoTpLink *link, uint8_t *payload, const uint16_t payload_size, uint16_t *out_size);

#ifdef __cplusplus
}
#endif

#endif // __ISOTP_H__


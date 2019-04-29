#ifndef __ISOTP_H__
#define __ISOTP_H__

#include <stdio.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

#include "isotp_defines.h"
#include "isotp_config.h"
#include "isotp_user.h"

typedef struct {
    /* sender paramters */
    uint32_t                    send_arbitration_id; /* used to reply consecutive frame */
    /* message buffer */
    uint8_t                     *send_buffer;
    uint16_t                    send_buf_size;
    uint16_t                    send_size;
    uint16_t                    send_offset;
    /* multi-frame flags */
    uint8_t                     send_sn;
    uint16_t                    send_bs_remain; /* Remain block size */
    uint8_t                     send_st_min;    /* Separation Time between consecutive frames, unit msec */
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
    uint8_t                     *receive_buffer;
    uint16_t                    recevie_buf_size;
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
} IsoTpLink;

/* init isotplink called when startup */
void isotp_init_link(IsoTpLink *link, uint32_t sendid, 
                     uint8_t *sendbuf, uint16_t sendbufsize,
                     uint8_t *recvbuf, uint16_t recvbufsize);

/* called periodicity, send consecutive frame, handle timeout, etc. */
void isotp_poll(IsoTpLink *link);

/* called when receive can message */
void isotp_on_can_message(IsoTpLink *link, uint8_t *data, uint8_t len);

/* send message, send immediately when deal with single frame message, 
   for multi-frame message, consecutive frams are send by isotp_poll */
int isotp_send(IsoTpLink *link, const uint8_t payload[], uint16_t size);

/* for functional addressing only */
int isotp_send_with_id(IsoTpLink *link, uint32_t id, const uint8_t payload[], uint16_t size);

/* receive message from buffer */
int isotp_receive(IsoTpLink *link, uint8_t *payload, const uint16_t payload_size, uint16_t *out_size);

#ifdef __cplusplus
}
#endif

#endif // __ISOTP_H__


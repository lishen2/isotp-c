#include <stdint.h>
#include "debug.h"
#include "isotp.h"
	
/* st_min to microsecond */
static uint8_t isotp_ms_to_st_min(uint8_t ms) 
{
    uint8_t st_min;

    st_min = ms;
    if (st_min > 0x7F){
        st_min = 0x7F;
    }

    return st_min;
}

/* st_min to msec  */
static uint8_t isotp_st_min_to_ms(uint8_t st_min) 
{
    uint8_t ms;
    
    if (st_min >= 0xF1 && st_min <= 0xF9){
        ms = 1;
    } else if (st_min <= 0x7F) {
        ms = st_min;
    } else {
        ms = 0;
    }

    return ms;
}

static int isotp_send_flow_control(IsoTpLink* link, uint8_t flow_status, uint8_t block_size, uint8_t st_min_ms) 
{

    IsoTpCanMessage message;
    int ret;

    /* setup message  */
    message.as.flow_control.type = ISOTP_PCI_TYPE_FLOW_CONTROL_FRAME;
    message.as.flow_control.FS = flow_status;
    message.as.flow_control.BS = block_size;
    message.as.flow_control.STmin = isotp_ms_to_st_min(st_min_ms);

    /* send message */
#ifdef ISO_TP_FRAME_PADDING
    (void)memset(message.as.flow_control.reserve, 0, sizeof(message.as.flow_control.reserve));
    ret = IsoTpUserSendCan(link->send_arbitration_id,
                           message.as.data_array.ptr, 
                           sizeof(message));
#else    
    ret = IsoTpUserSendCan(link->send_arbitration_id,
                           message.as.data_array.ptr, 
                           3);
#endif

    return ret;
}

static int isotp_send_single_frame(IsoTpLink* link, uint32_t id) 
{

	IsoTpCanMessage message;
    int ret;

    /* multi frame message length must greater than 7  */
    assert(link->send_size <= 7);

    /* setup message  */
    message.as.single_frame.type = ISOTP_PCI_TYPE_SINGLE;
    message.as.single_frame.SF_DL = (uint8_t)link->send_size;
    (void)memcpy(message.as.single_frame.data, link->send_buffer, link->send_size);

    /* send message */
#ifdef ISO_TP_FRAME_PADDING
    (void)memset(message.as.single_frame.data + link->send_size,
           0,
           sizeof(message.as.single_frame.data) - link->send_size);
    ret = IsoTpUserSendCan(id,
                           message.as.data_array.ptr, 
                           sizeof(message));
#else
    ret = IsoTpUserSendCan(id,
                           message.as.data_array.ptr, 
                           link->send_size + 1);
#endif

    return ret;
}

static int isotp_send_first_frame(IsoTpLink* link, uint32_t id) 
{
    
	IsoTpCanMessage message;
    int ret;

    /* multi frame message length must greater than 7  */
    assert(link->send_size > 7);

    /* setup message  */
    message.as.first_frame.type = ISOTP_PCI_TYPE_FIRST_FRAME;
    message.as.first_frame.FF_DL_low  = (uint8_t)link->send_size;
    message.as.first_frame.FF_DL_high = (uint8_t)(0x0F & (link->send_size >> 8));
    (void)memcpy(message.as.first_frame.data, 
           link->send_buffer, 
           sizeof(message.as.first_frame.data));

    /* send message */
    ret = IsoTpUserSendCan(id,
                           message.as.data_array.ptr, 
                           sizeof(message));
    if (ISOTP_RET_OK == ret){
        link->send_offset += sizeof(message.as.first_frame.data);
        link->send_sn = 1;
    }

    return ret;
}

static int isotp_send_consecutive_frame(IsoTpLink* link) 
{
    
	IsoTpCanMessage message;
    uint8_t data_length;
    int ret;

    /* multi frame message length must greater than 7  */
    assert(link->send_size > 7);

    /* setup message  */
    message.as.consecutive_frame.type = TSOTP_PCI_TYPE_CONSECUTIVE_FRAME;
    message.as.consecutive_frame.SN = link->send_sn;
    data_length = link->send_size - link->send_offset;
    if (data_length > sizeof(message.as.consecutive_frame.data)){
        data_length = sizeof(message.as.consecutive_frame.data);
    }
    (void)memcpy(message.as.consecutive_frame.data, link->send_buffer + link->send_offset, data_length);

    /* send message */
#ifdef ISO_TP_FRAME_PADDING
    (void)memset(message.as.consecutive_frame.data + data_length,
           0,
           sizeof(message.as.consecutive_frame.data) - data_length);
    ret = IsoTpUserSendCan(link->send_arbitration_id,
                           message.as.data_array.ptr,
                           sizeof(message));
#else
    ret = IsoTpUserSendCan(link->send_arbitration_id,
                           message.as.data_array.ptr,
                           data_length + 1);
#endif
    if (ISOTP_RET_OK == ret){
        link->send_offset += data_length;
        if (++(link->send_sn) > 0x0F){
            link->send_sn = 0;
        }
    }
    
    return ret;
}

int isotp_send(IsoTpLink *link, const uint8_t payload[], uint16_t size)
{
    return isotp_send_with_id(link, link->send_arbitration_id, payload, size);
}

int isotp_send_with_id(IsoTpLink *link, uint32_t id, const uint8_t payload[], uint16_t size)
{
    int ret;
    
    if (size > ISO_TP_MAX_MESSAGE_SIZE){
        IsoTpUserDebug("Message to big, increase ISO_TP_MAX_MESSAGE_SIZE to set a bigger buffer\n");
        return ISOTP_RET_OVERFLOW;
    }

    if (ISOTP_SEND_STATUS_INPROGRESS == link->send_status){
        IsoTpUserDebug("Abord previous message, which is sending in pregress\n");
    }

    /* copy into local buffer */
    link->send_arbitration_id = id;

    link->send_size = size;
    link->send_offset = 0;
    (void)memcpy(link->send_buffer, payload, size);

    if(link->send_size < 8){
        /* send single frame */
        ret = isotp_send_single_frame(link, id);
    } else {
        /* send multi-frame */
        ret = isotp_send_first_frame(link, id); 
        
        /* init multi-frame control flags */
        if (ISOTP_RET_OK == ret){
            link->send_bs_remain = 0;
            link->send_st_min = 0;
            link->send_wtf_count = 0;
            link->send_timer_st = IsoTpUserGetMs();
            link->send_timer_bs = IsoTpUserGetMs() + ISO_TP_DEFAULT_RESPONSE_TIMEOUT;
            link->send_protocol_resault = ISOTP_PROTOCOL_RESAULT_OK;
            link->send_status = ISOTP_SEND_STATUS_INPROGRESS;
        }
    }

    return ret;
}

static int isotp_receive_single_frame(IsoTpLink *link, IsoTpCanMessage *message)
{   
    /* copying data */
    (void)memcpy(link->receive_buffer, 
           message->as.single_frame.data, 
           message->as.single_frame.SF_DL);
    link->receive_size = message->as.single_frame.SF_DL;
    
    return ISOTP_RET_OK;
}

static int isotp_receive_first_frame(IsoTpLink *link, IsoTpCanMessage *message)
{
    uint16_t payload_length;

    /* check data length */
    payload_length = message->as.first_frame.FF_DL_high;
    payload_length = (payload_length << 8) + message->as.first_frame.FF_DL_low;
    if (payload_length > ISO_TP_MAX_MESSAGE_SIZE){
		IsoTpUserDebug("Multi-frame response too large for receive buffer.");
        return ISOTP_RET_OVERFLOW;
    }
    
    /* copying data */
    (void)memcpy(link->receive_buffer, message->as.first_frame.data, sizeof(message->as.first_frame.data));
    link->receive_size = payload_length;
    link->receive_offset = sizeof(message->as.first_frame.data);
    link->receive_sn = 1;

    return ISOTP_RET_OK;
}

static int isotp_receive_consecutive_frame(IsoTpLink *link, IsoTpCanMessage *message)
{
    uint16_t remaining_bytes;
    
    /* check sn */
    if (link->receive_sn != message->as.consecutive_frame.SN){
        return ISOTP_RET_WRONG_SN;
    }

    /* copying data */
    remaining_bytes = link->receive_size - link->receive_offset;
    if (remaining_bytes > sizeof(message->as.consecutive_frame.data)){
        remaining_bytes = sizeof(message->as.consecutive_frame.data);
    }
    (void)memcpy(link->receive_buffer + link->receive_offset, 
           message->as.consecutive_frame.data,
           remaining_bytes);

    link->receive_offset += remaining_bytes;
    if (++(link->receive_sn) > 0x0F){
        link->receive_sn = 0;
    }

    return ISOTP_RET_OK;
}

void isotp_on_can_message(IsoTpLink *link, uint8_t *data, uint8_t len)
{
    IsoTpCanMessage message;
    int ret;
    
    if (len < 2 || len > 8){
        return;
    }

    (void)memcpy(message.as.data_array.ptr, data, len);
    (void)memset(message.as.data_array.ptr + len, 0, sizeof(message.as.data_array.ptr) - len);

    switch(message.as.common.type) {
        case ISOTP_PCI_TYPE_SINGLE: {
            /* update protocol resault */
            if (ISOTP_RECEIVE_STATUS_INPROGRESS == link->receive_status){
                link->receive_protocol_resault = ISOTP_PROTOCOL_RESAULT_UNEXP_PDU;
            } else {
                link->receive_protocol_resault = ISOTP_PROTOCOL_RESAULT_OK;
            }

            /* handle message */
            ret = isotp_receive_single_frame(link, &message);
            if (ISOTP_RET_OK == ret){
                /* change status */
                link->receive_status = ISOTP_RECEIVE_STATUS_FULL;
            }
            break;
        }
        case ISOTP_PCI_TYPE_FIRST_FRAME: {
            /* update protocol resault */
            if (ISOTP_RECEIVE_STATUS_INPROGRESS == link->receive_status){
                link->receive_protocol_resault = ISOTP_PROTOCOL_RESAULT_UNEXP_PDU;
            } else {
                link->receive_protocol_resault = ISOTP_PROTOCOL_RESAULT_OK;
            }

            /* handle message */
            ret = isotp_receive_first_frame(link, &message);

            /* if overflow happend */
            if (ISOTP_RET_OVERFLOW == ret){
                /* update protocol resault */
                link->receive_protocol_resault = ISOTP_PROTOCOL_RESAULT_BUFFER_OVFLW;
                /* change status */
                link->receive_status = ISOTP_RECEIVE_STATUS_IDLE;
                /* send error message */								
                isotp_send_flow_control(link, PCI_FLOW_STATUS_OVERFLOW, 0, 0);
                break;
            }

            /* if receive successful */
            if (ISOTP_RET_OK == ret){
                /* change status */
                link->receive_status = ISOTP_RECEIVE_STATUS_INPROGRESS;
                /* send fc frame */
                link->receive_bs_count = ISO_TP_DEFAULT_BLOCK_SIZE;					
                isotp_send_flow_control(link, PCI_FLOW_STATUS_CONTINUE, link->receive_bs_count, ISO_TP_DEFAULT_ST_MIN);
                /* refresh timer cs */
                link->receive_timer_cr = IsoTpUserGetMs() + ISO_TP_DEFAULT_RESPONSE_TIMEOUT;
            }
            
            break;
        }
        case TSOTP_PCI_TYPE_CONSECUTIVE_FRAME: {
            /* check if in receiving status */
            if (ISOTP_RECEIVE_STATUS_INPROGRESS != link->receive_status){
                link->receive_protocol_resault = ISOTP_PROTOCOL_RESAULT_UNEXP_PDU;
                break;
            }

            /* handle message */
            ret = isotp_receive_consecutive_frame(link, &message);

            /* if wrong sn */
            if (ISOTP_RET_WRONG_SN == ret){
                link->receive_protocol_resault = ISOTP_PROTOCOL_RESAULT_WRONG_SN;
                link->receive_status = ISOTP_RECEIVE_STATUS_IDLE;
                break;
            }

            /* if success */
            if (ISOTP_RET_OK == ret){
                /* refresh timer cs */
                link->receive_timer_cr = IsoTpUserGetMs() + ISO_TP_DEFAULT_RESPONSE_TIMEOUT;
                
                /* receive finished */
                if (link->receive_offset >= link->receive_size){
                    link->receive_status = ISOTP_RECEIVE_STATUS_FULL;
                } else {
                    /* send fc when bs reaches limit */
                    if (0 == --link->receive_bs_count){
                        link->receive_bs_count = ISO_TP_DEFAULT_BLOCK_SIZE;											
                        isotp_send_flow_control(link, PCI_FLOW_STATUS_CONTINUE, link->receive_bs_count, ISO_TP_DEFAULT_ST_MIN);
                    }
                }
            }
            
            break;
        }
        case ISOTP_PCI_TYPE_FLOW_CONTROL_FRAME:
            /* handle fc frame only when sending in progress  */
            if (ISOTP_SEND_STATUS_INPROGRESS != link->send_status){
                break;
            }

            /* refresh bs timer */
            link->send_timer_bs = IsoTpUserGetMs() + ISO_TP_DEFAULT_RESPONSE_TIMEOUT;

            /* overflow */
            if (PCI_FLOW_STATUS_OVERFLOW == message.as.flow_control.FS) {
                link->send_protocol_resault = ISOTP_PROTOCOL_RESAULT_BUFFER_OVFLW;
                link->send_status = ISOTP_SEND_STATUS_ERROR;
            } 
            /* wait */
            else if (PCI_FLOW_STATUS_WAIT == message.as.flow_control.FS) {
                link->send_wtf_count += 1;
                /* wati exceed allowed count */
                if (link->send_wtf_count > ISO_TP_MAX_WFT_NUMBER){
                    link->send_protocol_resault = ISOTP_PROTOCOL_RESAULT_WFT_OVRN;
                    link->send_status = ISOTP_SEND_STATUS_ERROR;
                }
            } 
            /* permit send */
            else if (PCI_FLOW_STATUS_CONTINUE == message.as.flow_control.FS){
                link->send_bs_remain = message.as.flow_control.BS;
                link->send_st_min = isotp_st_min_to_ms(message.as.flow_control.STmin);
                link->send_wtf_count = 0;
            }
            break;
        default:
            break;
    };
    
    return;
}

int isotp_receive(IsoTpLink *link, uint8_t *payload, const uint16_t payload_size, uint16_t *out_size)
{
    uint16_t copylen;
    
    if (ISOTP_RECEIVE_STATUS_FULL != link->receive_status){
        return ISOTP_RET_NO_DATA;
    }

    copylen = link->receive_size;
    if (copylen > payload_size){
        copylen = payload_size;
    }

    memcpy(payload, link->receive_buffer, copylen);
    *out_size = copylen;

    link->receive_status = ISOTP_RECEIVE_STATUS_IDLE;

    return ISOTP_RET_OK;
}

void isotp_init_link(IsoTpLink *link, uint32_t sendid) {
    memset(link, 0, sizeof(*link));
    link->receive_status = ISOTP_RECEIVE_STATUS_IDLE;
    link->send_status = ISOTP_SEND_STATUS_IDLE;
    link->send_arbitration_id = sendid;
    return;
}

void isotp_poll(IsoTpLink *link)
{
    int ret;

    /* only polling when operation in progress */
    if (ISOTP_SEND_STATUS_INPROGRESS == link->send_status){

        /* continue send data */
        if (link->send_bs_remain > 0 && 
            (0 == link->send_st_min || (0 != link->send_st_min && IsoTpTimeAfter(IsoTpUserGetMs(), link->send_timer_st)))){
            
            ret = isotp_send_consecutive_frame(link);
            if (ISOTP_RET_OK == ret){
                link->send_bs_remain -= 1;
                link->send_timer_bs = IsoTpUserGetMs() + ISO_TP_DEFAULT_RESPONSE_TIMEOUT;
                link->send_timer_st = IsoTpUserGetMs() + link->send_st_min;

                /* check if send finish */
                if (link->send_offset >= link->send_size){
                    link->send_status = ISOTP_SEND_STATUS_IDLE;
                }
            } else {
                link->send_status = ISOTP_SEND_STATUS_ERROR;
            }
        }

        /* check timeout */
        if (IsoTpTimeAfter(IsoTpUserGetMs(), link->send_timer_bs)){
            link->send_protocol_resault = ISOTP_PROTOCOL_RESAULT_TIMEOUT_BS;
            link->send_status = ISOTP_SEND_STATUS_ERROR;
        }
    }

    /* only polling when operation in progress */
    if (ISOTP_RECEIVE_STATUS_INPROGRESS == link->receive_status){
        
        /* check timeout */
        if (IsoTpTimeAfter(IsoTpUserGetMs(), link->receive_timer_cr)){
            link->receive_protocol_resault = ISOTP_PROTOCOL_RESAULT_TIMEOUT_CR;
            link->receive_status = ISOTP_RECEIVE_STATUS_IDLE;
        }
    }

    return;
}


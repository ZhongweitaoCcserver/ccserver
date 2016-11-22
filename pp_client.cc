/* pp_client.cc -- 
 * Copyright (c) 2016--, ZhongWeiTao 
 * All rights reserved.
 */
#include "pp_client.h"
#include "log.h"
#include "epollreactor.h"
#include "bankhall.pb.h"
#include "codec.h"
#include "anet.h"

client_ptr pp_client::create_client(int cfd, int msg_size, int connect_ok)
{
    //CCSERVERLOG(CLL_DEBUG,  "pp_client::create_client.");
    client_ptr new_client(new pp_client(cfd, msg_size, connect_ok));
    return new_client;
}

pp_client::pp_client(int cfd, int msg_size, int connect_ok) 
    : client(cfd),
    _msg_size(msg_size),
    _connected_ok(false)
{
    if (_msg_size <=0 ) _msg_size =1;
    if (_msg_size >500  || _msg_size <=0 ) _msg_size =500;
        
    char err_msg[CODEC_ERR_MSG_MAXLEN];
    
    int ret_redis_connect = anet_check_noblock_connect(err_msg, _cfd);
    if (ret_redis_connect == ANET_ERR){
        //CCSERVERLOG(CLL_VERBOSE,  "pp_client error condition on socket for SYNC: %s", err_msg);
        return;
    }
    //CCSERVERLOG(CLL_DEBUG,  "pp_client::pp_client(int cfd) connect_ok:%d", connect_ok);
    
    if (connect_ok == 0 || ret_redis_connect == ANET_OK){
        _connected_ok = true;
        //CCSERVERLOG(CLL_NOTICE,  "pp_client::pp_client(int cfd) connect already success !");
        //offer ball now;
        _offer_ball();
    }
}
                 

int pp_client::_offer_ball()
{
    ccserver::withdraw  msg;
    msg.set_bus_msg_id("WITHDRAW01001");
    msg.set_serial_no("SERIALNO1201001");
    msg.set_card_no("CARD_NO_999999998");
    msg.set_card_no("PP_CLIENT_8888888");
    msg.set_amount(89898);
    msg.set_customer("mymind");
    msg.set_time("sdfsdfdsfdfsdf");
    
    char err_msg[CODEC_ERR_MSG_MAXLEN];
    memset(err_msg, 0, sizeof(err_msg));
    for (int i=0; i < _msg_size; ++i ){
        //CCSERVERLOG(CLL_DEBUG,  "pp_client::_offer_ball(), before:%d", i);        
        message_code(_out_sds_buf, msg, err_msg);
        //CCSERVERLOG(CLL_DEBUG,  "pp_client::_offer_ball(), after:%d", i);   
    }
    //CCSERVERLOG(CLL_DEBUG,  "pp_client::_offer_ball(), client::write_out_data()");   
    return client::write_out_data();
}

int pp_client::process_fire_event(epoll_event fire_event, int *keepalive_sec)
{
    char err_msg[CODEC_ERR_MSG_MAXLEN];
    //CCSERVERLOG(CLL_DEBUG,  "pp_client process_fire_event!");
    
    if (false == _connected_ok)
    {
        int ret_redis_connect = anet_check_noblock_connect(err_msg, _cfd);
        if (ret_redis_connect == ANET_ERR){
            CCSERVERLOG(CLL_VERBOSE,  "pp_client error condition on socket for SYNC: %s", err_msg);
            return ANET_ERR;
        }
        _connected_ok = true;
        //CCSERVERLOG(CLL_DEBUG,  "pp_client connect success!");
        //offer ball now;
        return _offer_ball();
    }
    
    return client::process_fire_event(fire_event, keepalive_sec);
}
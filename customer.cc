/* customer.cc -- 
 * Copyright (c) 2016--, ZhongWeiTao 
 * All rights reserved.
 */
#include "customer.h"
#include "log.h"
//
int customer::read_in_data()
{
    int ret = client::read_in_data();
    
    size_t len = _in_sds_buf.size();
    if (_in_sds_buf_peak < len) _in_sds_buf_peak = len;
    
    return ret;
}


//support protobuf message
//message_ptr message_decode(cpp_sds& in_cpp_sds, size_t *unpack_len, char *err_msg)
//this is much more effective than msg_queue
int customer::process_fire_event(epoll_event fire_event, int *keepalive_sec)
{
    int ret = read_in_data();
    if (ret != CLI_OK)
        return ret;
    
    if (_in_sds_buf.size() == 0)
        return CLI_OK;
    
    char err_msg[CODEC_ERR_MSG_MAXLEN];
    memset(err_msg, 0, sizeof(err_msg));
    
    size_t unpack_len = 0;
    while(1)
    {   
        //CCSERVERLOG(CLL_DEBUG,  "customer::process_fire_event.");
        //message_ptr  new_msg_ptr = message_decode(_in_sds_buf,  err_msg);
        message_ptr  new_msg_ptr = message_decode(_in_sds_buf, &unpack_len, err_msg);
        if (nullptr == new_msg_ptr){
            break;
        }
        
        message_ptr  reply_msg = _skills_pkg.on_message(new_msg_ptr);
        //CCSERVERLOG(CLL_DEBUG,  "customer::process_fire_event.");
        if (reply_msg != nullptr){//         
            message_code(_out_sds_buf, *reply_msg, err_msg);
        }
    }
    //CCSERVERLOG(CLL_DEBUG,  "customer::process_fire_event.");
    size_t remain_len = _in_sds_buf.size() - unpack_len;   
    if (remain_len == 0){
        _in_sds_buf.clear();
    }else if (unpack_len != 0 ){//still have some data to be handle(data is not enough to a full message.), need to read next time.   
        //in_data = sdscpylen(in_data, in_data+unpack_len, remain_len);
        //_in_sds_buf.reset(in_data);
        _in_sds_buf.cpylen(_in_sds_buf.c_sds + unpack_len, remain_len);
    }
    
    if (_out_sds_buf.size() > 0) {
        return write_out_data();
    }
    
    return CLI_OK;
}


/*
//message_decode(cpp_sds& in_cpp_sds, msg_queue& dec_msg_queue, char *err_msg);
//to be fix, some message ok, some err.
int customer::process_fire_event(epoll_event fire_event, int *keepalive_sec)
{
    //typedef std::queue<message_ptr> msg_queue; //define in codec.h
    msg_queue _pending_msg_queue;
        
    int ret = read_in_data();
    if (ret != CLI_OK)
        return ret;
    
    if (_in_sds_buf.size() == 0)
        return CLI_OK;
    
    char err_msg[CODEC_ERR_MSG_MAXLEN];
    memset(err_msg, 0, sizeof(err_msg));
    message_decode(_in_sds_buf, _pending_msg_queue, err_msg);// all msg keep in _pending_msg_queue;
    
    while(!_pending_msg_queue.empty())
    {
        message_ptr  new_msg_ptr =  _pending_msg_queue.front();
        _pending_msg_queue.pop();
        message_ptr  reply_msg = _skills_pkg.on_message(new_msg_ptr);
        memset(err_msg, 0, sizeof(err_msg));
        message_code(_out_sds_buf, *reply_msg, err_msg);
    }
    
    if (_out_sds_buf.size() > 0) {
        return write_out_data();
    }
    
    return CLI_OK;
}*/

extern skills_package customer_skills_pkg;
//port::set_create_client_fun
client_ptr customer::create_client(int cfd)
{
    //CCSERVERLOG(CLL_DEBUG,  "customer::create_client.");
    client_ptr new_client(new customer(cfd, customer_skills_pkg));
    return new_client;
}




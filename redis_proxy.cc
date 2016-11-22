/* redis_proxy.cc -- 
 * Copyright (c) 2016--, ZhongWeiTao 
 * All rights reserved.
 */
#include <algorithm>
#include "redis_proxy.h"
#include "log.h"

client_ptr redis_proxy::create_client(int cfd, string& redis_server_addr, int redis_server_port, string& source_addr)
{
    CCSERVERLOG(CLL_DEBUG,  "redis_proxy::create_client.");
    client_ptr new_client(new redis_proxy(cfd, redis_server_addr, redis_server_port, source_addr));
    return new_client;
}

int redis_proxy::register_events(int efid, int *cfds, int *cfds_len)
{
    if (*cfds_len < 2) return CLI_ERR;
    if (_redis_server_addr.size() ==0) return CLI_ERR;
    
    char errmsg[ANET_ERR_LEN];
    
    //connect to redis server first!
    int connect_ok = -1;
    if (_source_addr.size() > 0){
        _redis_cfd = anet_tcp_noblock_best_bind_connect(errmsg, &connect_ok, _redis_server_addr.c_str(), _redis_server_port, _source_addr.c_str());
    }
    else{
        _redis_cfd = anet_tcp_noblock_best_bind_connect(errmsg, &connect_ok, _redis_server_addr.c_str(), _redis_server_port, NULL);
    }
    //check, 
    if (connect_ok == 0) _redis_connected = true;
    
    if (_redis_cfd == ANET_ERR) {
        CCSERVERLOG(CLL_WARNING, "redis_client anet_tcp_noblock_best_bind_connect:%s, redis_server_addr:%s,  redis_server_port:%d, source_addr:%s",
                                   errmsg, _redis_server_addr.c_str(),  _redis_server_port, _source_addr.c_str());
        return ANET_ERR;
    }
   
    
    int ret = anet_epoll_in_add(errmsg, efid, _cfd);
    if (ret == ANET_ERR){
        CCSERVERLOG(CLL_WARNING, "redis_client anet_epoll_in_del:%s", errmsg);
        return ANET_ERR;
    }
    
    ret = anet_epoll_in_add(errmsg, efid, _redis_cfd);
    if (ret == ANET_ERR){
        CCSERVERLOG(CLL_WARNING, "redis_client anet_epoll_in_del:%s", errmsg);
        return ANET_ERR;
    }
    
    cfds[0] = _cfd;
    cfds[1] = _redis_cfd;
    *cfds_len = 2;
    return CLI_OK;
}

int redis_proxy::_send_to_redix()
{
    //send data to _redis_cfd directly.
    char errmsg[ANET_ERR_LEN];
    int ret = anet_write_sds_to_fid(errmsg, _redis_cfd, _in_sds_buf );
    if (ret == ANET_ERR){
        CCSERVERLOG(CLL_VERBOSE,  "anet_write_sds_to_fid:%s", errmsg);
    }
    
    return ret;         
}

int redis_proxy::_send_to_client()
{
    return client::write_out_data();    
}

int redis_proxy::_process_fire_event_cfd(int *keepalive_sec)
{
    //read cfd into _in_sds_buf
    int ret = read_in_data();
    if (ret != CLI_OK)
        return ret;
    
    if (_in_sds_buf.size() == 0)
        return CLI_OK;
    //send to redix directly.
    return _send_to_redix();
}

int redis_proxy::_process_fire_event_redis(int *keepalive_sec)
{
    //read cfd into _out_sds_buf
    char errmsg[ANET_ERR_LEN];
    
    if (false == _redis_connected)
    {
        int ret_redis_connect = anet_check_noblock_connect(errmsg, _redis_cfd);
        if (ret_redis_connect == ANET_ERR){
            CCSERVERLOG(CLL_VERBOSE,  "redis_proxy error condition on socket for SYNC: %s", errmsg);
            return ANET_ERR;
        }
        _redis_connected = true;
        return ANET_OK;
    }
    
    //read cfd into _out_sds_buf
    int ret = anet_read_fid_to_sds(errmsg, _redis_cfd, _out_sds_buf );
    if (ret == ANET_ERR){
        CCSERVERLOG(CLL_VERBOSE,  "anet_read_fid_to_sds:%s", errmsg);
        return ANET_ERR;
    }
        
    if (_out_sds_buf.size() == 0)
        return CLI_OK;
    
    //send to client directly.
    return _send_to_client();    
}

int redis_proxy::write_to_client()
{
    int ret = CLI_OK;
    if (_in_sds_buf.size() > 0){
        ret = _send_to_redix();
    }
    
    if (ret != CLI_OK)
        return ret;
    
    if (_out_sds_buf.size() > 0){
        ret = _send_to_client();
    }
    
    return ret;
}

int redis_proxy::process_fire_event(epoll_event fire_event, int *keepalive_sec)
{
    if (fire_event.data.fd == _cfd) {
        return _process_fire_event_cfd(keepalive_sec);
    } else if (fire_event.data.fd == _redis_cfd) {
        return _process_fire_event_redis(keepalive_sec);
    }
    return CLI_OK;
}
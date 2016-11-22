/* telnetecho.cc -- 
 * Copyright (c) 2016--, ZhongWeiTao 
 * All rights reserved.
 */
#include <algorithm>
#include "telnetecho.h"
#include "log.h"
#include "epollreactor.h"

client_ptr telnetecho::create_client(int cfd)
{
    //CCSERVERLOG(CLL_DEBUG,  "telnetecho::create_client.");
    client_ptr new_client(new telnetecho(cfd));
    return new_client;
}

static const char* find_crlf(const sds in_sds)
{
    const char const_crlf[] = "\r\n";
    const char *ptr = const_crlf;
    
    char *end = in_sds + sdslen(in_sds);
    const char* crlf = std::search(in_sds, end, ptr, ptr+2);
    return crlf == end ? NULL : crlf;
}

int telnetecho::process_fire_event(epoll_event fire_event, int *keepalive_sec)
{
    int ret = read_in_data();
    //CCSERVERLOG(CLL_DEBUG,  "telnetecho::process_fire_event. ret:%d, sds:%s", ret, _in_sds_buf.c_sds);
    if (ret != CLI_OK)
        return ret;
    
    if (_in_sds_buf.size() == 0)
        return CLI_OK;
    
    const char c_shutdown[]="SHUTDOWN";
    int cmd_shutdown = memcmp(_in_sds_buf.c_sds, c_shutdown, sizeof(c_shutdown)-1);
    size_t line_len;
    while(1)
    {        
        const char*  new_line = find_crlf(_in_sds_buf.c_sds );
        if (nullptr == new_line){
            break;
        }
        
        line_len = new_line - _in_sds_buf.c_sds;
        //process bussness msg.
        _out_sds_buf.cat("ccserver echo:");
        _out_sds_buf.catlen(_in_sds_buf.c_sds, line_len +2);
        //CCSERVERLOG(CLL_DEBUG,  "telnetecho::process_fire_event. %s", _in_sds_buf.c_sds);
        
        size_t remain_len = _in_sds_buf.size() - line_len -2;
        if (remain_len <= 0 ){
            _in_sds_buf.clear();
            break;
        }
        else{
            _in_sds_buf.cpylen(new_line+2, remain_len);
        }       
    }
    
    *keepalive_sec = 300; //five minus alive. 
    if (_out_sds_buf.size() > 0) {
        int ret = write_out_data();
        if (0 == cmd_shutdown){
            CCSERVERLOG(CLL_NOTICE,  "telnetecho::call shutdown!.");
            ccserver::epollreactor::stop_all_reactors();
        }
        return ret;
    }
    
    return CLI_OK;
}
/* redis_proxy.cc -- 
 * Copyright (c) 2016--, ZhongWeiTao 
 * All rights reserved.
 */
#ifndef CCSERVER_REDIS_PROXY_H
#define CCSERVER_REDIS_PROXY_H
#include <iostream>
using namespace std;
#include "client.h"

//not finish
class redis_proxy : public client
{
    public:
        redis_proxy(int cfd, string& redis_server_addr, int redis_server_port, string& source_addr)
                 : client(cfd),
                   _redis_server_addr(redis_server_addr),
                   _redis_server_port(redis_server_port),
                   _source_addr(source_addr),            //anet_tcp_noblock_best_bind_connect
                   _redis_connected(false)
                 {
                     //cout << "redis_proxy:: creating!" << endl;
                 };
        //int read_in_data(); 
        int process_fire_event(epoll_event fire_event, int *keepalive_sec);   //decode and process message.
        int write_to_client();  //check send to redix vs send to client. 
        int register_events(int efid, int *cfds, int *cfds_len); 
        
        static client_ptr create_client(int cfd, string& redis_server_addr, int redis_server_port, string& source_addr);
        
        redis_proxy(redis_proxy const& other)=delete;
        redis_proxy& operator=(redis_proxy const& other)=delete; 
    private:
        int _send_to_redix();
        int _send_to_client();
        int _process_fire_event_cfd(int *keepalive_sec);
        int _process_fire_event_redis(int *keepalive_sec);    
        int _redis_cfd;
        string _redis_server_addr;
        int _redis_server_port;
        string _source_addr;
        bool _redis_connected;
};


#endif



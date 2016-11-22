/* pp_client.cc -- 
 * Copyright (c) 2016--, ZhongWeiTao 
 * All rights reserved.
 */
#ifndef CCSERVER_PP_CLIENT_H
#define CCSERVER_PP_CLIENT_H
#include <iostream>
using namespace std;
#include "client.h"

class pp_client : public client
{
    public:
        pp_client(int cfd, int msg_size, int ret_conn);
        //int read_in_data(); 
        int process_fire_event(epoll_event fire_event, int *keepalive_sec);  //decode and process message.
        //int write_to_client();  client::write_to_client is ok. 
        static client_ptr create_client(int cfd, int msg_size, int ret_conn);
    private:
        int _offer_ball();
        int _msg_size;
        bool _connected_ok;
};

#endif
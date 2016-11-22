/* telnetecho.cc -- 
 * Copyright (c) 2016--, ZhongWeiTao 
 * All rights reserved.
 */
#ifndef CCSERVER_TELNET_ECHO_H
#define CCSERVER_TELNET_ECHO_H
#include <iostream>
using namespace std;
#include "client.h"

//not finish
class telnetecho : public client
{
    public:
        telnetecho(int cfd)
                 : client(cfd)
                 {
                     //cout << "telnetecho:: creating!" << endl;
                 };
        //int read_in_data(); 
        int process_fire_event(epoll_event fire_event, int *keepalive_sec);   //decode and process message.
        //int write_to_client();  client::write_to_client is ok. 
        
        static client_ptr create_client(int cfd);
    private:
};


#endif

/* customer.h -- 
 * Copyright (c) 2016--, ZhongWeiTao 
 * All rights reserved.
 */
#ifndef CCSERVER_CUSTOMER_H
#define CCSERVER_CUSTOMER_H
#include <iostream>
using namespace std;
#include "client.h"
#include "codec.h"
#include "skills_pkg.h"

//not finish
class customer : public client
{
    public:
        customer(int cfd, skills_package& skills_pkg)
                 : client(cfd),
                   _skills_pkg(skills_pkg),
                   _in_sds_buf_peak(0)
                 {
                     //cout << "customer:: creating!" << endl;
                 };
        int read_in_data(); 
        int process_fire_event(epoll_event fire_event, int *keepalive_sec);  //decode and process message.
        //int write_to_client();  client::write_to_client is ok. 
        
        static client_ptr create_client(int cfd);
    private:
        skills_package& _skills_pkg;
        size_t _in_sds_buf_peak;    /* Recent (100ms or more) peak of querybuf size. */
        
};


#endif


/* client.cc -- 
 * Copyright (c) 2016--, ZhongWeiTao 
 * All rights reserved.
 */
#ifndef CCSERVER_CLIENT_H
#define CCSERVER_CLIENT_H
#include <memory>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include "sds.h"

#include "anet.h" 

#define NET_IP_STR_LEN 46          /* INET6_ADDRSTRLEN is 46, but we need to be sure */
#define DEFAULT_IOBUF_LEN 512
#define PROTO_MAX_IOBUF_LEN  65536 /* 64kb max query buffer. */

#define CLI_OK                 ANET_OK
#define CLI_ERR                ANET_ERR
#define CLI_PENDING_WRITE      ANET_PENDING_WRITE
#define CLI_ERR_LEN            ANET_ERR_LEN


//
//Difference busness has difference client. 
class client;
typedef std::shared_ptr<client> client_ptr;

class client: public std::enable_shared_from_this<client>
{
    public:
        explicit client(int cfd):_cfd(cfd){};
        virtual ~client(){close(_cfd);};  //base class. must be virtual
        
        //register_events is called by epollreator to provide a efid to client to register concerned events.
        //And, what's the event type, register_events never care.
        //in this class, only register EPOLLIN, and the default behavior is Level Triggered.
        //If it doesn't meet your requirements, overwrite it in your class, and it is necessary that your class based on client!
        //Parameters: 
        //      efid--epollreator's epoll_create efid.
        //      cfds--out para, a fid array,
        //      cfds_len--in_out para, in: *cfds_len is the buff len of cfds (the max number of cfd can be register in epoll.); 
        //                            out: how many cfds(save in cfds para) have been registerd. 
        virtual int register_events(int efid, int *cfds, int *cfds_len);
        
        //Parameters: 
        //      keepalive_sec --out para, notify epollreator remove *this after keepalive_sec if *this is idled. -1 means alive forever!
        //      fire_event    --in  para, data was created by epoll_wait.
        //process_fire_event is called by epollreator when a event of this client is fired,  which is register before.
        //epollreator never care about what your event is, it means that you should parse it by yourself, and call your event handle directly. 
        //In this class(client), it provides an implement of pingpong test. 
        //Consider thread_safed, args is designed in passed_by_value. 
        //Return CLI_OK if success, and the client is still valid.
        //Return CLI_ERR if the fid of client is close or something wrong, then  then client will be destroyed. 
        //Return CLI_PENDING_WRITE if write is block(EAGAIN), then mark it and write again next it.    
        virtual int process_fire_event(epoll_event fire_event, int *keepalive_sec)
                   { 
                       // Do pingpong test. Copy data from _in_sds_buf to _out_sds_buf, then clear data in _in_sds_buf.
                       // Data in _out_sds_buf will be sent back in write_out_data().
                       int ret = read_in_data();
                       if (ret != CLI_OK)
                           return ret;
                       
                       //_out_sds_buf = sdscatsds(_out_sds_buf, _in_sds_buf);
                       //sdsclear(_in_sds_buf);
                       _out_sds_buf.cat(_in_sds_buf);
                       _in_sds_buf.clear();
                       *keepalive_sec = -1;  //alive seconds. -1 means alive forever!, Notice: cann't change keepalive_sec from a positive number to -1;
                       return write_out_data();                       
                   };      
        
        //Read data to _in_sds_buf from client. Return CLI_OK if read success.
        //Return CLI_ERR if the fid of client is closed or something wrong, then client will be destroyed. 
        virtual int read_in_data();
        
        //Write data in _out_sds_buf to client. Return CLI_OK if success, and the client is still valid.
        //Return CLI_ERR if the fid of client is close or something wrong, then  then client will be destroyed. 
        //Return CLI_PENDING_WRITE if write is block(EAGAIN), then mark it and write again next it.
        virtual int write_out_data();
        
        //create_client is provided to tcp_port.
        //A different port means a different busness, and a different means a different type of client.
        //epollreator[0] owns tcp_ports, when a new connection is established. tcp_ports will call this function to get a new client;
        //epollreator_poll dispatch the client. then call set_cip, set_cport, and most important, call register_events.
        static client_ptr create_client(int cfd);
        
        int get_cfd(){return _cfd; };
        void get_cip(char *cip, size_t cip_len){ strncpy(cip, _cip, cip_len); };
        int get_cport(){return _cport;};
        
        void set_cip(char *cip){strncpy(_cip, cip, sizeof(NET_IP_STR_LEN)); };
        void set_cport(int cport){ _cport = cport; };
        // No copyable.
        client(client const& other)=delete;
        client& operator=(client const& other)=delete;
    protected:
        int _cfd;
        char _cip[NET_IP_STR_LEN];
        int _cport;
        cpp_sds _in_sds_buf;
        cpp_sds _out_sds_buf;
};

#endif
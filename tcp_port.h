/* tcp_port.h -- 
 * Copyright (c) 2016--, ZhongWeiTao 
 * All rights reserved.
 */
#ifndef CCSERVER_PORT_H
#define CCSERVER_PORT_H
#include <string>
using std::string;
#include "client.h"

class tcp_port
{
    public:
        typedef std::function<client_ptr (int cfd) > create_client_fun;
        tcp_port(int port_number, const string &_bind_addr, const string _bussness_name)
            :_port_number(port_number),
             _bind_addr(_bind_addr),
             _bussness_name(_bussness_name),
             _socket_fid(-1), //pending
             _create_client(client::create_client) //default pingpong client;            
            {};
        ~tcp_port(){if (_socket_fid != -1) close(_socket_fid); };
        //create_client will be called in ccserver's accept event.
        void set_create_client_fun(const create_client_fun& create_client){ _create_client = create_client; };
        
        int get_port_number(){return _port_number;};
        string get_bind_addr(){return _bind_addr;};
        string get_bussness_name(){return _bussness_name;};
        void set_socket_fid(int socket_fid){ _socket_fid = socket_fid; };
        int get_socket_fid(){return _socket_fid;};
        client_ptr new_client(int cfd) const
        {
            //if (nullptr == _create_client)
            //    return nullptr;            
            return _create_client(cfd);    
        };
    private:
        int _port_number;
        string _bind_addr;
        string _bussness_name;
        int _socket_fid; //listen socket_fid
        create_client_fun _create_client; 
};

#endif

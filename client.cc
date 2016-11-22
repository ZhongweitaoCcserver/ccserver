/* client.cc -- 
 * Copyright (c) 2016--, ZhongWeiTao 
 * All rights reserved.
 */
#include "client.h"
#include "log.h"

using namespace std;  

//port::set_create_client_fun
client_ptr client::create_client(int cfd)
{
    //cout << "client::create_client was called" << endl;
    client_ptr new_client(new client(cfd));
    return new_client;
}
                 

// Read data to _in_sds_buf from client. Return CLI_OK if read success.
// Return CLI_ERR if the fid of client is closed or something wrong, then client will be destroyed. 
int client::read_in_data()
{
    char errmsg[ANET_ERR_LEN];
    int ret = anet_read_fid_to_sds(errmsg, _cfd, _in_sds_buf );
    if (ret == ANET_ERR){
        //CCSERVERLOG(CLL_VERBOSE,  "anet_read_fid_to_sds:%s", errmsg);
        return ANET_ERR;
    }
    return CLI_OK;
}

// Write data in _out_sds_buf to client. Return CLI_OK if success, and the client is still valid.
// Return CLI_ERR if the fid of client is close or something wrong, then  then client will be destroyed. 
// Return CLI_PENDING_WRITE if write is block(EAGAIN), then mark it and write again next it.
int client::write_out_data()
{
    char errmsg[ANET_ERR_LEN];
    int ret = anet_write_sds_to_fid(errmsg, _cfd, _out_sds_buf );
    if (ret == ANET_ERR){
        CCSERVERLOG(CLL_VERBOSE,  "anet_write_sds_to_fid:%s", errmsg);
        return ANET_ERR;
    }
    return ret;//CLI_OK, or  CLI_PENDING_WRITE   
}
//_efid, &client_cfds, *client_cfds_len
int client::register_events(int efid, int *cfds, int *cfds_len)
{
    if (*cfds_len < 1) return CLI_ERR;
    
    char errmsg[ANET_ERR_LEN];
    //CCSERVERLOG(CLL_DEBUG, "anet_epoll_in_add:efid:%d _cfd:%d", efid, _cfd);
    int ret = anet_epoll_in_add(errmsg, efid, _cfd);
    if (ret == ANET_ERR){
        CCSERVERLOG(CLL_WARNING, " anet_epoll_in_del:%s", errmsg);
        return ANET_ERR;
    }
    
    cfds[0] = _cfd;
    *cfds_len = 1;
    return CLI_OK;
}

#ifdef CLIENT_UNIT_TEST_MAIN
#include "tcp_port.h"
#include "customer.h"
#include "bankhall_skills.h"

skills_package customer_skills_pkg(onUnknownMessageType);

int main()
{
    customer_skills_pkg.register_msg_skill<ccserver::withdraw>(cash_withdraw);
    customer_skills_pkg.register_msg_skill<ccserver::deposit>(cash_deposit);
    customer_skills_pkg.register_msg_skill<ccserver::outsourcing>(on_outsourcing);
    tcp_port bank_port(9999, "127.0.0.1", "pingpong");
    bank_port.set_create_client_fun(customer::create_client); //should be called by main.
    bank_port.set_socket_fid(19235); //should be called by server.   
    client_ptr a_bank_cli = bank_port.new_client(12345);
    
    tcp_port pp_port(9999, "127.0.0.1", "pingpong");
    //pp_port.set_create_client_fun(client::create_client); //should be called by main. default is pp client.
    pp_port.set_socket_fid(19234); //should be called by server.
    
    client_ptr a_pp_cli = pp_port.new_client(12345);
    return 0;
}



#endif


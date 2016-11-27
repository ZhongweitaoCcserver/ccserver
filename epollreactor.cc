/* epollreactor.cc -- 
 * Copyright (c) 2016--, ZhongWeiTao 
 * All rights reserved.
 */
#include <iostream>
#include <set>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "epollreactor.h"
#include "log.h"
#include "anet.h"

using namespace ccserver;
using namespace std;

    
//epollreactor static data.
tcp_port_map                                epollreactor::_port_map;
std::vector<std::shared_ptr<epollreactor> > epollreactor::_reactors;
bool                                        epollreactor::_all_stop = false;
std::condition_variable                     epollreactor::_cv_all_stop;
std::mutex                                  epollreactor::_mutex_all_stop;
int                                         epollreactor::_timer_interval_sec = -1; // if _timer_bucket_cnt < 0; don't check Keepalive Client.
int                                         epollreactor::_timer_bucket_cnt =10;   //_timer_interval_sec * _timer_bucket_cnt == _circle_time_sec;
int                                         epollreactor::_circle_time_sec =60;    //_timer_interval_sec * _timer_bucket_cnt == _circle_time_sec;
//
void epollreactor::create_reactors(int cnt)
{
    if (cnt <=0) cnt = 1;
    
    for (int i=0; i< cnt; ++i) {
        std::shared_ptr<epollreactor> new_reactor_ptr(new epollreactor());
        _reactors.push_back(new_reactor_ptr);
    }        
}

//Generally called in main. 
//reactors[0] accept all connect request. and dispatch connection(client) to each reactor.
//So, _accept_tcp_handler does not need to be thread_safe.
void epollreactor::add_tcp_port(tcp_port_ptr& new_port)
{
    char errmsg[ANET_ERR_LEN];
    
    if (get_reactors_cnt() ==0){
        CCSERVERLOG(CLL_WARNING, "Please call create_reactors() first!");
        return;
    }

    string bind_addr = new_port->get_bind_addr();  
    string bussness_name = new_port->get_bussness_name();    
    int fd;
    if (bind_addr.find(':') == string::npos ){
        fd = anet_tcp_server(errmsg, new_port->get_port_number(), bind_addr.c_str(), AF_INET, LISTENQ); //ipv4
    }else{
        fd = anet_tcp_server(errmsg, new_port->get_port_number(), bind_addr.c_str(), AF_INET6, LISTENQ); //ipv6
    }        
    
    if (fd == ANET_ERR){
        CCSERVERLOG(CLL_WARNING, "add_tcp_port:%s port:%d addr:%s bussness:%s",  
                                errmsg, new_port->get_port_number(), bind_addr.c_str(), bussness_name.c_str() );
        return;
    }
    
    anet_non_block(errmsg, fd);
    
    new_port->set_socket_fid(fd); 
    _port_map.insert( make_pair(fd, new_port));
    _reactors[0]->_add_listen_id(fd);         
}

void epollreactor::add_client(client_ptr &new_client)
{
    static int which_reactor = 0;   //notice, it's safe, never mind.
    
    //CCSERVERLOG(CLL_DEBUG, "_add_client_nolock ");
    if (_reactors.size() > 1) {
        ++which_reactor;
        which_reactor = which_reactor % _reactors.size();
    }
    
    if ( which_reactor != 0)
    {    //muti reactor, muti thread. dispatch it to every reactors.
        _reactors[which_reactor]->_add_client_lock(new_client);//thread_safe call. 
    } else{//same thread, more efficiently.
        //CCSERVERLOG(CLL_DEBUG, "_add_client_nolock ");
        _reactors[0]->_add_client_nolock(new_client);
    }     
}

//As add_tcp_port is only called in main(). so, _accept_tcp_handler is called only by _reactors[0].
//_reactors[0] accept all connect request, and dispatch new connection(client) to each reactor.
//So, if the new client is dispatched to other reactor thar it's necessary to call add_client_lock.
//Otherwise, if the new client is dispatched to _reactors[0] himself, add_client_nolock is more efficiently.
void epollreactor::_accept_tcp_handler(int fd) 
{
    char errmsg[ANET_ERR_LEN];
    
    //stop to accept new client.
    if (_all_stop) return;
    
    auto it = _port_map.find(fd);
    if ( _port_map.end() == it)
        return ;
    
    tcp_port_ptr cur_port = it->second;
    
    //static int which_reactor = 0;   //notice, it's safe, never mind.
    int max = MAX_ACCEPTS_PER_CALL;
    int cfd, cport;
    char cip[NET_IP_STR_LEN];
    memset(cip, 0, sizeof(cip));
    while(max--) 
    {   
        cfd = anet_accept(errmsg, fd, cip, sizeof(cip), &cport);
        //CCSERVERLOG(CLL_DEBUG, "ccserver trace epollreactor::_accept_tcp_handler() cfd:%d ", cfd);
        if (cfd == ANET_ERR){
            if (errno != EWOULDBLOCK){
                CCSERVERLOG(CLL_WARNING, "Accepting client connection:%s", errmsg);
                }            
            return ;
        }
        
        anet_non_block(errmsg, cfd);
        client_ptr new_cli_ptr = cur_port->new_client(cfd);
        if (new_cli_ptr == nullptr) return;
        
        new_cli_ptr->set_cip(cip);
        new_cli_ptr->set_cport(cport);
        
        add_client(new_cli_ptr);// static epollreactor::add_client
    }
}

void epollreactor::run_all_reactors()
{
    //if (_reactors.empty() || _port_map.empty())
    if (_reactors.empty() )
        return;
    
    for (auto it_reactor : _reactors){
        it_reactor->start_work_loop();        
    }

}

void epollreactor::_stop_work_loop()
{
    for (auto fid : _listen_fids){
        close(fid);//close listen_id first!
    }    
    _stop_flag = true; 
}; 

void epollreactor::stop_all_reactors()
{
    CCSERVERLOG(CLL_NOTICE,  "CCSERVER shutdown now!");
    _all_stop = true;
    _cv_all_stop.notify_all();
    for (auto it_reactor : _reactors){
        it_reactor->_stop_work_loop();        
    }
}


void epollreactor::wait_all_reactors()
{
    for (auto it_reactor : _reactors){
        it_reactor->_wait_working_thread();        
    }
}

bool epollreactor::wait_for_stop(int wait_out_sec)
{
    std::unique_lock<std::mutex> lck(_mutex_all_stop);
    //cv.wait_for(lck,std::chrono::seconds(wait_out_sec)) == std::cv_status::timeout 
    _cv_all_stop.wait_for(lck, std::chrono::seconds(wait_out_sec)) ;
    return _all_stop;    
}


void epollreactor::_add_listen_id(int fid)
{
    char errmsg[ANET_ERR_LEN];
    int ret = anet_epoll_in_add(errmsg, _efid, fid);
    if (ret == ANET_ERR)
    {
        CCSERVERLOG(CLL_WARNING, "epoll listen id failed!!! %s", errmsg);
        exit(-1);
    }
    _listen_fids.insert(fid);    
}


void epollreactor::_add_client_lock(client_ptr &new_client)
{
    std::lock_guard<std::mutex> c_lock(_clients_pending_mutex);    
    _clients_pending.push_back(new_client);//push_back in an vector, and process at the beginging of epoll loop.
}

//called at the beginging of each epoll_wait loop.
void epollreactor::_add_all_client()
{
    clients local_clients;
    
    if  (_clients_pending.size() == 0)
        return;
    
    {
        std::lock_guard<std::mutex> c_lock(_clients_pending_mutex); 
        local_clients.swap(_clients_pending);    
    }
    
    for (auto a_cli: local_clients){
        _add_client_nolock(a_cli);
    }
}

/*
void epollreactor::_add_client_nolock(client_ptr &new_client)
{
    int  client_cfds[MAX_CLIENT_FD_NUM];    
    int client_cfds_len = MAX_CLIENT_FD_NUM;
    int ret = new_client->register_events(_efid, client_cfds, &client_cfds_len);
    if (ret == ANET_ERR){
        CCSERVERLOG(CLL_WARNING, "register event failed: %s"); 
        return;
    }
    //CCSERVERLOG(CLL_DEBUG, "register event success: %d", client_cfds_len); 
    //int cfd = new_client->get_cfd(); , _cl_map.insert(make_pair(cfd, new_client) ); 
    for (int idx = 0; idx < client_cfds_len; ++idx)
    {
        _cl_map.insert(make_pair(client_cfds[idx], new_client) ); 
        //
        if (idx >0){
            _cl_fid_mmap.insert(make_pair(client_cfds[0], client_cfds[idx]));  
        }
        //
    }    
}
*/
void epollreactor::_add_client_nolock(client_ptr &new_client)
{
    std::vector<int> client_cfds;
    
    int ret = new_client->register_events(_efid, client_cfds);
    if (ret == ANET_ERR){
        CCSERVERLOG(CLL_WARNING, "register event failed."); 
        return;
    }
    
    size_t client_cfds_size = client_cfds.size();
    if (client_cfds_size ==0 )
    {
        CCSERVERLOG(CLL_WARNING, "register event failed, client_cfds_size invalid: 0"); 
        return;
    }

    int master_fid = client_cfds[0];
    _cl_map.insert(make_pair(master_fid, new_client) );    
    for (size_t idx = 1; idx < client_cfds_size; ++idx) {
        _cl_map.insert(make_pair(client_cfds[idx], new_client) );  
        _cl_fid_mmap.insert(make_pair(client_cfds[0], client_cfds[idx]));  
    }    
}

void epollreactor::_remove_client(int cfd, bool timer_fired)
{
    char errmsg[ANET_ERR_LEN];

    //CCSERVERLOG(CLL_DEBUG, "_remove_client cfd:%d", cfd);
    auto it_cli = _cl_map.find(cfd);
    if (it_cli == _cl_map.end())
        return;
    
    int master_cfd =  (it_cli->second)->get_cfd();     
    auto position = _cl_fid_mmap.lower_bound(master_cfd);  
    while(position != _cl_fid_mmap.upper_bound(master_cfd))  
    {  
        int ret = anet_epoll_in_del(errmsg, _efid, position->second);
        if (ret == ANET_ERR){
            CCSERVERLOG(CLL_WARNING, " anet_epoll_in_del:%s", errmsg);
        } 
        _cl_map.erase(position->second);
        
        if ( (not timer_fired) || (cfd != position->second) ) 
        {
            //need to remove cfd from _spare_wheel and _in_use_wheel;
            size_t ret = _spare_wheel.erase(position->second);
            if (ret <=0) _in_use_wheel.erase(position->second);
        }
        ++position;
    } 
    
    _cl_map.erase(master_cfd);
    _cl_fid_mmap.erase(master_cfd);  
    if ( (not timer_fired) || (cfd != master_cfd) )  
    {
            //need to remove cfd from _spare_wheel and _in_use_wheel;
            size_t ret = _spare_wheel.erase(master_cfd);
            if (ret <=0) _in_use_wheel.erase(master_cfd);
    }    
}

/*
void epollreactor::_remove_client(int cfd, bool timer_fired)
{
    char errmsg[ANET_ERR_LEN];

    //CCSERVERLOG(CLL_DEBUG, "_remove_client cfd:%d", cfd);
    if (not timer_fired)
    {
        //need to remove cfd from _spare_wheel and _in_use_wheel;
        size_t ret = _spare_wheel.erase(cfd);
        if (ret <=0) _in_use_wheel.erase(cfd);
    }
    
    auto it_cli = _cl_map.find(cfd);
    if (it_cli == _cl_map.end())
        return;
        
    int ret = anet_epoll_in_del(errmsg, _efid, cfd);
    if (ret == ANET_ERR){
        CCSERVERLOG(CLL_WARNING, " anet_epoll_in_del:%s", errmsg);
        //skip error!!!
    }
    _cl_map.erase(it_cli);    
}*/

void epollreactor::_work_loop()
{
    //MessagePtr new_task;   
    int nfds;
    struct epoll_event events[MAX_SIMULTANOUS_EVENTS];
    std::vector<int> client_write_pending;
    std::vector<int> client_write_pending_delay;
    
    //const int events_size = sizeof(events)/sizeof(struct epoll_event);
    //notify main(manager. boss) that everything is ready;
    
    int alive_sec =-1;
    for(;;)//it is a loop work in thread;
	{
        if ( _stop_flag )
			break;
        
        _add_all_client();
        nfds = epoll_wait(_efid, events, MAX_SIMULTANOUS_EVENTS, 1000);//timeout, fixed to an arg, milliseconds, 200=0.2 sec.
        if (nfds < 0)
        {
            CCSERVERLOG(CLL_DEBUG, " %s", strerror(errno));
            continue;
        }
        //CCSERVERLOG(CLL_DEBUG, " epoll_wait:%d", _efid);
        
        for (int idx = 0; idx < nfds; ++idx)
        {
            //CCSERVERLOG(CLL_DEBUG, " fired event fid:%d", events[idx].data.fd);            
            auto it_cli = _cl_map.find(events[idx].data.fd);
            if (it_cli != _cl_map.end() )
            {
                alive_sec = -1;
                int ret = it_cli->second->process_fire_event(events[idx], &alive_sec);
                if (ret == CLI_ERR){
                    _remove_client(events[idx].data.fd, false);
                    continue;
                }else if (ret == CLI_PENDING_WRITE){
                    client_write_pending.push_back(events[idx].data.fd);
                }
                
                if ((_timer_interval_sec > 0) &&( alive_sec >= 0))  {
                    int cfd = it_cli->second->get_cfd(); //a client's master_cfd.
                    _update_client_alive_sec(cfd, alive_sec);
                }
            }
            else
            {
                auto it = _listen_fids.find(events[idx].data.fd);           
                if (it != _listen_fids.end())
                {
                    //call server accept, and add_epoll 
                    _accept_tcp_handler(events[idx].data.fd);
                    //CCSERVERLOG(CLL_DEBUG, "ccserver trace.");
                }
               continue;
            }
        }
        
        if (client_write_pending.size() == 0)
        {
            if ((_timer_interval_sec > 0) && _timer_fired) {
                _remove_dead_client();
            }   
            continue;
        }
        //process pending_write_clients  
        //client_write_pending_delay.clear(); no need, backup is awlays empty.      
        for (auto fid: client_write_pending)
        {
            auto it_cli = _cl_map.find(fid);
            if (it_cli == _cl_map.end() )
            {
                CCSERVERLOG(CLL_WARNING, "client data not fund!!!");
                continue;
            }
            
            int ret = it_cli->second->write_out_data();
            if (ret == CLI_OK){
                continue;
            }else if (ret == CLI_ERR) {
                _remove_client(fid, false);
            }else if (ret == CLI_PENDING_WRITE) {//to be fixed.
                client_write_pending_delay.push_back(fid);
            }    
        }
        client_write_pending.clear();
        client_write_pending.swap(client_write_pending_delay);   

        //kick off idle connect!  to be fixed!
        if ((_timer_interval_sec > 0) && _timer_fired) {
            _remove_dead_client();
        }        
    }
}

void epollreactor::_start_timer()
{
     _timer_thread = std::shared_ptr<std::thread> (new std::thread(std::bind(&epollreactor::_timer_loop, this)));    
}

// if _timer_bucket_cnt <= 0; don't check Keepalive Client.
// bucket_cnt must > 0
void epollreactor::set_keepalive_args(int interval_sec, int bucket_cnt)
{
    if (interval_sec <=0 ) {
        _timer_interval_sec = -1;
    }else {
        _timer_interval_sec = interval_sec;
    } // if _timer_bucket_cnt <= 0; don't check Keepalive Client.
    
    if (bucket_cnt > 0){    
        _timer_bucket_cnt = bucket_cnt;     //_timer_interval_sec * _timer_bucket_cnt == recycle time; 
    }
     _circle_time_sec = interval_sec * bucket_cnt;
    
    if (interval_sec <= 0) {return;}
    
    for (auto it_reactor : _reactors){
        it_reactor->_start_timer();        
    }     
}


void epollreactor::_timer_loop()
{
    //wait_for_stop(_timer_interval_sec) return _all_stop.
    while ( epollreactor::wait_for_stop(_timer_interval_sec) == false){
        _timer_fired = true;   //set _timer_fired every _timer_interval_sec.
    }
} 

void epollreactor::_wait_working_thread()
{
    if ( _timer_thread !=nullptr && _timer_thread->joinable()) {
        _timer_thread->join();
    }
    
    if ( _working_thread !=nullptr && _working_thread->joinable()) {
        _working_thread->join();
    }
}


//int                                         epollreactor::_timer_interval_sec = -1; // if _timer_bucket_cnt < 0; don't check Keepalive Client.
//int                                         epollreactor::_timer_bucket_cnt =10;   //_timer_interval_sec * _timer_bucket_cnt == _circle_time_sec;
//called after process_fire_event.
void epollreactor::_update_client_alive_sec(int cfd, int alive_sec)
{
    int remain_sec = (_timer_bucket_cnt - _cur_bucket_idx) * _timer_interval_sec;
       
    if (alive_sec >= remain_sec ){
        _in_use_wheel.erase(cfd);//erase by key, no need check!
        //save in _spare_wheel;
        //CCSERVERLOG(CLL_DEBUG, "keep in _spare_wheel; alive_sec:%d,  remain_sec:%d", alive_sec, remain_sec);
        alive_sec -= remain_sec;
        _spare_wheel[cfd] = alive_sec;
        return;
    }   
    //CCSERVERLOG(CLL_DEBUG, "keep in _insert_in_use_wheel;");
    _insert_in_use_wheel(cfd, alive_sec);
}


void epollreactor::_insert_in_use_wheel(int cfd, int alive_sec)
{
    int idx = (alive_sec + _timer_interval_sec ) / _timer_interval_sec  -1 ;
    
    if ( idx > _timer_bucket_cnt ) {return;}
    //if ( idx < 0 ) idx = 0;
    
    //CCSERVERLOG(CLL_DEBUG, "_insert_in_use_wheel alive_sec:%d, idx:%d _timer_interval_sec:%d", alive_sec, idx, _timer_interval_sec);
    idx = ( _cur_bucket_idx + idx  ) % _timer_bucket_cnt;
    //CCSERVERLOG(CLL_DEBUG, "_insert_in_use_wheel _cur_bucket_idx:%d, idx:%d _timer_bucket_cnt:%d", _cur_bucket_idx, idx, _timer_bucket_cnt);
    //
    _in_use_wheel.insert(make_pair(cfd, idx) );
}

//to be fixed.
void epollreactor::_remove_dead_client()
{
    _timer_fired = false;
    //CCSERVERLOG(CLL_DEBUG, "_remove_dead_client.");
    for(auto it = _in_use_wheel.begin(); it != _in_use_wheel.end(); ) 
    {
        if (it->second == _cur_bucket_idx){
            _remove_client(it->first, true);
            it = _in_use_wheel.erase(it); //only for C++11, C++98 not support!
        }else {
            ++it;
        }
    }
    
    ++_cur_bucket_idx;
    _cur_bucket_idx = _cur_bucket_idx % _timer_bucket_cnt;
    if (_cur_bucket_idx != 0) { return;}
    //CCSERVERLOG(CLL_DEBUG, "_remove_dead_client. process _spare_wheel.");
    //move data from _spare_wheel; //_in_use_wheel one circle time. _circle_time_sec
    for(auto it = _spare_wheel.begin(); it != _spare_wheel.end(); ) 
    {
        if (it->second < _circle_time_sec)
        {   //move it to _insert_in_use_wheel
            //CCSERVERLOG(CLL_DEBUG, "move data from _spare_wheel.");
            _insert_in_use_wheel(it->first, it->second);
            it = _spare_wheel.erase(it); //only for C++11, C++98 not support!
        }else {
            it->second -= _circle_time_sec;  
            ++it;
        }
    }
}
 

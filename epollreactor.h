/* epollreactor.h -- 
 * Copyright (c) 2016--, ZhongWeiTao 
 * All rights reserved.
 */
#ifndef CCSERVER_EPOLLREACTOR_H
#define CCSERVER_EPOLLREACTOR_H
#include <sys/epoll.h>
#include <thread>
#include <functional>
#include <mutex>
#include <vector>
#include <list>
#include <utility> 
#include <memory>
#include <map>
#include <set>
#include <string>
#include <chrono>             // std::chrono::seconds
#include <mutex>              // std::mutex, std::unique_lock
#include <condition_variable> // std::condition_variable, std::cv_status
#include "tcp_port.h"
#include "client.h"
#define MAX_CLIENT_FD_NUM    5   //the max epoll_cfd number for each client 
namespace ccserver
{
#define LISTENQ 10000
#define MAX_ACCEPTS_PER_CALL 1000

#define MAX_SIMULTANOUS_EVENTS  5000

using std::string;
typedef std::function<void(int)>  accept_tcp_func;
typedef std::shared_ptr<tcp_port> tcp_port_ptr;
typedef std::map<int, tcp_port_ptr> tcp_port_map;  //listen_fid is the key.

//warpper of thread.
typedef std::map<int, client_ptr> client_map; 
typedef std::map<int, int> timer_wheel; //client_cfd, bucket_idx;
typedef std::vector<client_ptr> clients; 
class epollreactor
{
    public:
        static void create_reactors(int cnt);
        static int  get_reactors_cnt(){return _reactors.size(); };
        static void add_tcp_port(tcp_port_ptr& new_port);
        //static void _accept_tcp_handler(int fd);       
        static void run_all_reactors();
        static void stop_all_reactors(); //send command
        static void wait_all_reactors(); //wait for stop.
        static bool wait_for_stop(int wait_out_sec);   //return _all_stop.
        static void set_keepalive_args(int interval_sec, int bucket_cnt); //call _set_keepalive_args for each reactor.
        static void add_client(client_ptr &new_client);        
        
        epollreactor():_loop_start(false),
                      _stop_flag(false),
                      _timer_fired(false),
                      _cur_bucket_idx(0)                      
                     { 
                         _efid = epoll_create1(EPOLL_CLOEXEC);
                     };

        ~epollreactor(){close(_efid);};
        void start_work_loop()
             {                
                 if (_loop_start) return;                                 
                 _loop_start = true;                 
                 _working_thread = std::shared_ptr<std::thread> (new std::thread(std::bind(&epollreactor::_work_loop, this)));
                 //working_thread->detach();//test ! ok
             };

        epollreactor(epollreactor const& other)=delete;
        epollreactor& operator=(epollreactor const& other)=delete; 
    private:
        static void _accept_tcp_handler(int fd); 
    
        static tcp_port_map _port_map;
        static std::vector<std::shared_ptr<epollreactor> > _reactors;
        static bool _all_stop;
        static std::condition_variable _cv_all_stop;
        static std::mutex _mutex_all_stop;
        static int _timer_interval_sec;  // if _timer_bucket_cnt < 0; don't check Keepalive Client.
        static int _timer_bucket_cnt;    //_timer_interval_sec * _timer_bucket_cnt == _circle_time_sec;
        static int _circle_time_sec; // _circle_time_sec = _timer_interval_sec * _timer_bucket_cnt
        
        
        void _insert_in_use_wheel(int cfd, int alive_sec);
        void _update_client_alive_sec(int cfd, int alive_sec);
        
        void _add_listen_id(int fid);
        void _add_client_lock(client_ptr &new_client);
        void _add_client_nolock(client_ptr &new_client);
        void _add_all_client();        
        void _remove_client(int cfd, bool timer_fired); //timer_fired: called from _remove_dead_client or not.
        void _work_loop();
        void _timer_loop();
        void _stop_work_loop();
        void _wait_working_thread();
        void _remove_dead_client();
        void _start_timer();
        
        bool _loop_start;
        bool _stop_flag;
        bool _timer_fired;
        int _efid; //epoll fid,
        
        //std::vector<int> _listen_fids;
        std::set<int> _listen_fids;
        client_map _cl_map;
        std::shared_ptr<std::thread> _working_thread; //_work_loop
        std::shared_ptr<std::thread> _timer_thread;  //run _timer_loop(), call by _start_timer();
        void _accept_tcp_handle(int fid);
        
        clients _clients_pending;            //for add_client
        std::mutex _clients_pending_mutex;   //for add_client
        
        timer_wheel _spare_wheel;            //client_cfd, _alive_time; last_alive_time > (_timer_interval_sec * _timer_bucket_cnt)
                                             //every circle _alive_time -= (_timer_interval_sec * _timer_bucket_cnt), 
                                             //if _alive_time < (_timer_interval_sec * _timer_bucket_cnt), move it to _in_use_wheel;
        timer_wheel _in_use_wheel;           //client_cfd, bucket_idx;
        int _cur_bucket_idx;                 //if _in_use_wheel.bucket_idx == _cur_bucket_idx, the client(cfd) will be freed!
        //vector<ThreadItem> threadlist;
	    //threadlist.resize(n);
};


}

#endif
/* server.cc -- 
 * Copyright (c) 2016--, ZhongWeiTao 
 * All rights reserved.
 */
#include <iostream>                                                                                                      
//#include <map>
#include <functional>   // std::bind
//#include <arpa/inet.h>
#include <signal.h>
#include "tcp_port.h"
#include "customer.h"
#include "telnetecho.h"
#include "redis_proxy.h"
#include "epollreactor.h"
#include "bankhall_skills.h"
#include "log.h"

using namespace std;
using namespace ccserver;

skills_package customer_skills_pkg(onUnknownMessageType);

static int shutdown_asap =0;
static void sigShutdownHandler(int sig) {
    const char *msg;

    switch (sig) {
    case SIGINT:
        msg = "Received SIGINT scheduling shutdown...";
        break;
    case SIGTERM:
        msg = "Received SIGTERM scheduling shutdown...";
        break;
    default:
        msg = "Received shutdown signal, scheduling shutdown...";
    };

    /* SIGINT is often delivered via Ctrl+C in an interactive session.
     * If we receive the signal the second time, we interpret this as
     * the user really wanting to quit ASAP without waiting to persist
     * on disk. */
    if (shutdown_asap && sig == SIGINT) {
        logger::log_from_handler(LL_WARNING, "You insist... exiting now.");
        exit(0); /* Exit with an error since this was not a clean shutdown. */
    }
    logger::log_from_handler(LL_WARNING, msg);
    epollreactor::stop_all_reactors();
    shutdown_asap = 1;
}


void setupSignalHandlers(void) {
    struct sigaction act;

    /* When the SA_SIGINFO flag is set in sa_flags then sa_sigaction is used.
     * Otherwise, sa_handler is used. */
    sigemptyset(&act.sa_mask);
    act.sa_flags = 0;
    act.sa_handler = sigShutdownHandler;
    sigaction(SIGTERM, &act, NULL);
    sigaction(SIGINT, &act, NULL);
    return;
}

//typedef std::function<client_ptr (int cfd) > create_client_fun; declare in tcp_port.h
#ifndef CCSERVER_PP_CLIENT
int main(int argc, char* argv[])
{
    int threads_cnt = 1;
    if (argc != 2)
    {
        fprintf(stderr, "Usage: %s  <threads>\n", argv[0]);
        fprintf(stderr, "Threads number default: 1\n");
    }
    else
    {
        threads_cnt = atoi(argv[1]);
    }
    
    logger::set_log_level(LL_VERBOSE);
    CCSERVERLOG(CLL_NOTICE,  "CCSERVER Begin! Threads Number: %d", threads_cnt);
    
    signal(SIGHUP, SIG_IGN);
    signal(SIGPIPE, SIG_IGN);
    setupSignalHandlers();
    
    //redis_proxy-- static client_ptr create_client(int cfd, string& redis_server_addr, int redis_server_port, string& source_addr);
    tcp_port::create_client_fun redis_proxy_client_create_fun = \
                 std::bind(redis_proxy::create_client, std::placeholders::_1, string("127.0.0.1"), 6379, string("127.0.0.1"));    
    std::shared_ptr<tcp_port> redis_proxy_port_ptr(new tcp_port(9996, "127.0.0.1", "redis_proxy") );
    redis_proxy_port_ptr->set_create_client_fun(redis_proxy_client_create_fun); 
    
    std::shared_ptr<tcp_port> telcho_port_ptr(new tcp_port(9997, "127.0.0.1", "telnetecho") );
    telcho_port_ptr->set_create_client_fun(telnetecho::create_client); 
    
    std::shared_ptr<tcp_port> pp_port_ptr(new tcp_port(9998, "127.0.0.1", "pingpong") );
    
    customer_skills_pkg.register_msg_skill<ccserver::withdraw>(cash_withdraw);
    customer_skills_pkg.register_msg_skill<ccserver::deposit>(cash_deposit);
    customer_skills_pkg.register_msg_skill<ccserver::outsourcing>(on_outsourcing);   
    std::shared_ptr<tcp_port> bank_port_ptr(new tcp_port(9999, "127.0.0.1", "bankhall") );
    bank_port_ptr->set_create_client_fun(customer::create_client); 
    
    epollreactor::create_reactors(threads_cnt);
    epollreactor::add_tcp_port(pp_port_ptr);
    epollreactor::add_tcp_port(bank_port_ptr);
    epollreactor::add_tcp_port(telcho_port_ptr);
    epollreactor::add_tcp_port(redis_proxy_port_ptr);
    
    epollreactor::run_all_reactors();
    epollreactor::set_keepalive_args(10, 12);//remove once every 10 sec. 2 minus a circle(move data from spare wheel).
            
    //to be fixed, waitcondiction 
    //twenty minus;
    bool early_stop = false;
    for (int loop_cnt = 0; loop_cnt < 20; ++loop_cnt)
    {
        early_stop = epollreactor::wait_for_stop(60);
        if (early_stop)//stop_all_reactors was called by others.
            break;
        CCSERVERLOG(CLL_NOTICE,  "CCSERVER %d minus passed.", loop_cnt+1);    
        //do something meanningful, or smell the flower. :)
    }
    
    if (not early_stop){
        CCSERVERLOG(CLL_NOTICE,  "CCSERVER time off now!");
        epollreactor::stop_all_reactors();//time off now!
    }
    CCSERVERLOG(CLL_NOTICE,  "CCSERVER wait for reactors.");
    epollreactor::wait_all_reactors();
    CCSERVERLOG(CLL_NOTICE,  "CCSERVER exit now!");   
    return 0;
}
#endif

#ifdef CCSERVER_PP_CLIENT
#include "anet.h"
#include "pp_client.h"
#include <atomic>
extern std::atomic<long> total_bytes_read; // define in anet.c
extern std::atomic<long> total_msgs_read;  // define in anet.c

int main(int argc, char* argv[])
{
    if (argc != 7)
    {
        fprintf(stderr, "Usage: client <host_ip> <port> <threads> <msg_size> <sessions> <time>\n");
        return 0;
    }

    logger::set_log_level(LL_VERBOSE);
    CCSERVERLOG(CLL_NOTICE,  "CCPP_CLIENT Begin!");
    
    signal(SIGHUP, SIG_IGN);
    signal(SIGPIPE, SIG_IGN);
    setupSignalHandlers();
    
    const char* host_ip = argv[1];
    uint16_t host_port = static_cast<uint16_t>(atoi(argv[2]));
    int threads_cnt = atoi(argv[3]);
    int msg_size = atoi(argv[4]);
    int session_cnt = atoi(argv[5]);
    int timeout = atoi(argv[6]);
    
    epollreactor::create_reactors(threads_cnt);    
    
    //connect to redis server first!
    int cfd = 0;
    char errmsg[ANET_ERR_LEN];
    for (int idx=0; idx < session_cnt; ++idx)
    {
        int connect_ok = -1;
        cfd  = anet_tcp_noblock_best_bind_connect(errmsg, &connect_ok, host_ip, host_port, "127.0.0.1");
        if (cfd == ANET_ERR) {
            CCSERVERLOG(CLL_WARNING, "pp_client anet_tcp_noblock_best_bind_connect:%s, host:%s,  port:%d",
                                       errmsg, host_ip,  host_port);
            return ANET_ERR;
        }
        client_ptr new_client_ptr = pp_client::create_client(cfd, msg_size, connect_ok);
        epollreactor::add_client(new_client_ptr);        
    }
    
    CCSERVERLOG(CLL_NOTICE, "All connected.");
    epollreactor::run_all_reactors();
    epollreactor::set_keepalive_args(60, 2);
    
    bool early_stop = false;
    for (int loop_cnt = 0; loop_cnt < timeout; ++loop_cnt)
    {
        early_stop = epollreactor::wait_for_stop(1);
        if (early_stop)//stop_all_reactors was called by others.
            break;
        //CCSERVERLOG(CLL_NOTICE,  "CCSERVER %d minus passed.", loop_cnt+1);    
        //do something meanningful, or smell the flower. :)
    }
    
    //sdt::atomic<long> total_bytes_read = 0; define in anet.c
    //sdt::atomic<long> total_msgs_read = 0; define in anet.c    
    if (not early_stop){
        CCSERVERLOG(CLL_NOTICE,  "CCSERVER time off now!");
        epollreactor::stop_all_reactors();//time off now!
    }    
    long _total_bytes_read = total_bytes_read;
    CCSERVERLOG(CLL_NOTICE, "****:%ld total bytes read.", _total_bytes_read);
    long _total_msgs_read = total_msgs_read;
    CCSERVERLOG(CLL_NOTICE, "****:%ld total messages read.", _total_msgs_read);
    long average_size = 0;
    if (_total_msgs_read !=0)
        average_size = _total_bytes_read / _total_msgs_read;
    CCSERVERLOG(CLL_NOTICE, "****:%ld average message size.", average_size);
    double throughput = static_cast<double>(total_bytes_read) / (timeout * 1024 * 1024); 
    CCSERVERLOG(CLL_NOTICE, "****:%lf MiB/s throughput.", throughput);
    int packages = static_cast<double>(_total_msgs_read) / (timeout); 
    CCSERVERLOG(CLL_NOTICE, "****:%ld Msg/s throughput.", packages);
    
    CCSERVERLOG(CLL_NOTICE,  "CCSERVER wait for reactors.");
    epollreactor::wait_all_reactors();
    CCSERVERLOG(CLL_NOTICE,  "CCSERVER exit now!");   
    return 0;
}
#endif


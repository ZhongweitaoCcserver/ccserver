关于 ccserver 
-----------
ccserver 是一个C++开发的, 可运行在Linux上的高并发服务器样例程序。主要运用非阻塞TCP, epoll技术，支持多线程。

ccserver 相比起网格上流行的libevent等流行的网络库, 在性能上更占优势，因为它对底层tcp各函数运用更直接。

ccserver 更大的优势在于使用上的便利性，采用面向对象方法。更简洁高效的实现, 无论是二次开发或者是维护上，其优势是不言而喻的。


先说性能上的优势，虽然它可能不是主要的，不过它能更直接说明问题: 

ccserver_test/single_thread_test_report.txt 这是一个与libevent的测试比较报告。

这里只摘录一部份作说明。

ccserver，ccpp_client: 这是我开发的ccserver, 和客户端;

libevent server , libevent client: 这是来自陈硕的测试程序, 他开发一个网络库moduo，与我的实现是大不相同的, 在github上你可以找到。

由于我找来libevent 的测试程序只支持单线程，我也只好用单线程来进行比较;

ccpp_client 的命令行参数msg_size即是一次数据发送，包含数据包的个数。

msg_size: 1 Threads: 1 Sessions: 1 timeout: 30     #单线程,包文大小109字节，单连接，测试30秒

*****************ccserver Vs ccpp_client *********************************************

4356: 21 Nov 19:29:54.392 * CCPP_CLIENT Begin! --server.cc:149                  #CASE_1, 服务器和客户端都是我的实现

4356: 21 Nov 19:29:54.393 * All connected. --server.cc:180

4356: 21 Nov 19:30:24.395 * CCSERVER time off now! --server.cc:197

4356: 21 Nov 19:30:24.395 * CCSERVER shutdown now! --epollreactor.cc:169

4356: 21 Nov 19:30:24.395 * ****:72061426 total bytes read. --server.cc:201

4356: 21 Nov 19:30:24.395 * ****:661114 total messages read. --server.cc:203

4356: 21 Nov 19:30:24.395 * ****:109 average message size. --server.cc:207       #每次发包，读包都是109个字节

4356: 21 Nov 19:30:24.396 * ****:2.290771 MiB/s throughput. --server.cc:209      #每秒读吞吐量, 2.29M /秒

4356: 21 Nov 19:30:24.396 * ****:22037 Msg/s throughput. --server.cc:211         #每秒读吞吐量, 22037 个包/秒

4356: 21 Nov 19:30:24.396 * CCSERVER wait for reactors. --server.cc:213

4356: 21 Nov 19:30:24.396 * CCSERVER exit now! --server.cc:215

------------------------------------------------------------------------------------

*****************libevent server Vs libevent client **********************************

timeout                                                                         #CASE_2, 服务器和客户端都是libevent的实现

49571347 total bytes read

454783 total messages read

109.000 average messages size                                                   #每次发包，读包都是109个字节

1.576 MiB/s throughtput                                                         #每秒读吞吐量, 1.576M /秒

15159 Msg/s throughput.                                                         #每秒读吞吐量, 15159 个包/秒

------------------------------------------------------------------------------------

#这两个对比， 我的实现比libevent在性能上是有大幅度的提升的。

*****************ccserver Vs libevent client ******************************************

timeout                                        #CASE_3 这是用libevent的客户商端连接我的服务端

56706051 total bytes read                      #可以看出也比CASE_2单纯用libevent快。

520239 total messages read

109.000 average messages size

1.803 MiB/s throughtput

17341 Msg/s throughput.                                              

------------------------------------------------------------------------------------

*****************libevent server Vs ccpp_client ************************************

4506: 21 Nov 19:31:30.432 * CCPP_CLIENT Begin! --server.cc:149                  #CASE_4 这是用我的客户商端连接libevent的服务端

4506: 21 Nov 19:31:30.434 * All connected. --server.cc:180                      #也比CASE_2单纯用libevent快。

4506: 21 Nov 19:32:00.436 * CCSERVER time off now! --server.cc:197

4506: 21 Nov 19:32:00.436 * CCSERVER shutdown now! --epollreactor.cc:169

4506: 21 Nov 19:32:00.436 * ****:63639650 total bytes read. --server.cc:201

4506: 21 Nov 19:32:00.436 * ****:583850 total messages read. --server.cc:203

4506: 21 Nov 19:32:00.436 * ****:109 average message size. --server.cc:207

4506: 21 Nov 19:32:00.436 * ****:2.023053 MiB/s throughput. --server.cc:209

4506: 21 Nov 19:32:00.436 * ****:19461 Msg/s throughput. --server.cc:211

4506: 21 Nov 19:32:00.436 * CCSERVER wait for reactors. --server.cc:213

4506: 21 Nov 19:32:00.436 * CCSERVER exit now! --server.cc:215

------------------------------------------------------------------------------------

++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

其它的测试结果(改变包文大小和连接数), 在我的single_thread_test_report.txt都有，无一例外都是我的实现完胜。


再说一下我的程序功能, 它完成了以下这些任务：

多端口多服务:9996，9997，9998，9999 四个端口，实现四种不同服务并发处理。

其中: 
9998 : pingpong  测试; 

9997 : 可以用telnet 链接，echo 功能(顺便实现SHUTDOWN 并闭服务功能，如执行脚本 shutdown_ccserver.sh 即可关闭ccserver); 
      
9999 : 处理google 的 protobuf 包; 这里是模拟处理一个银行存取款的交易。
      
9996 : 可实现代理功能, 如用作 redis 的转发代理(redis 客户端可以连到这个端口, 而后我的程序转发包到redis服务器, 获取服务器应答后再转发回redis客户端)。
      
多线程:调用epollreactor:: (threads_cnt)就可以创建threads_cnt个线程。

每个client还可以多连接:每个client连接上ccserver后，可以再去连接其它几个服务器, ccserver可以对一个client的多个fid监控事件。参见redis_proxy的实现。

空闲连接超时断开的处理:timer wheel 算法处理超时连接, 我做了变通处理，更加高效。

支持读一半或者写一半处理:每个client都有自己的接收发送缓冲。缓冲用redis的sds代码实现，我对sds进行了最小的C++封装，主要是实现RAII.

此外, 还修正了原代码中的一个小瑕疵(请看 sdscpylen() ， 说只是瑕疵, 不算bug, 是因为我查了整个源码, redis没有触碰到这个边界条件),

我用memmove代替了memcpy, 避免了地址复重叠memcpy可能的出错。 我写了测试代码, 地址重叠memcpy的确是会出错的, 在sds.c  最下面有我的测试代码。                                              
支持批处理: 处理了读一次读入多个业务包或者半个业务包的情形， 如9999端口, 客户端是可以一次发多个withdraw包的。 

在读取数据的处理上, 我是利用readv一次读入64K数据, 如果读入数据大于client原接收缓冲，缓冲是可以自动扩大的。

以上四个端口四种服务，它们的区别只在一个类上，客户端类的不同。 

相比libevent，我的实现体现了真正的面向对象编程，而如果用libevent来实现的话, 还得自已去设计程序的架构，为每个连接开辟接收发送缓冲，去处理线程等等。

ccserver的具体实现:

ccserver有三个核心类:tcp_port, client, epollreactor; 
   三者关系上: epollreactor 拥有多个tcp_port和client; tcp_port只有一个client的创建函数指针，不拥有client.
             
  tcp_port: 这个类设计比较简单, 主要用它来保存port端口信息, 
  
  主要说它两个成员: _socket_fid, 这个是对应tcp的listen_id, epollreactor拥有多个tcp_port，存放在tcp_port_map中，_socket_fid是这个map的Key值;
  
  _create_client: 这是一个函数指针, epollreactor调用它（通过new_client来间接调用）来创建归属于这个port的客户端实例.  
  
  在server.cc的main函数里, 可以看到这个函数指针被赋值为client的静态的instance factory函数。
  
  之所以采用函数指针而不是采用C++virtual虚函数和继承机制，是因为两个原因：
  
     其一, 不同的client构造函数他们的参数可能完全不同, virtual机制在这点上无法提供支持。
     
     而在client的构造函数上，提供给后来的继承类更大的灵活性上, 还是很有必要的;
     
     当然, 其实这些构造函数, 除了代表一个连接的connect_id, 其它的基本都可以是全局变量。
     
     其二, tcp_port没有那么多必要复杂化, 那怕是用C++的继承机制, 派生类玩不出新的花样，它的数据和功能都非常固定。
     
     反映在程序上, 一个服务端口的开启, socket, bind, listen, accept, 这些都是固定套路。就把它固定下来好了。
     
     不同的port，只对应不同的client，产生不同的具体业务的client, 其它都一样。
  
  client:   服务端口新来一个连接，就产生一个新的client; 不同的client，有不同的应用级协议和业务处理逻辑。
  
  这里我是采用了C++的virtual继承机制, 所有新的业务实现, 都要继承client类。
  
  为什么tcp_port采用函数指针，而client采用virtual呢？ 
  
  我对技术的理解是: 所有技术都有其优缺点, 两面性; 太灵活了，就难规范, 太规范了，势必影响灵活性。 要有取舍，合则用，不合则不用。
  
  client与epollreactor之间, 有一些互相调用的接口，我分析它们，觉得规范强一点，程序的健壮性会更好，对于灵活性的影响也是有限的或者说可以接受的.
  
  client与port相比，复杂太多, 而且，数据和功能, 针对不同业务都会有很大的扩展。
  
  client的基类也就实现了最小最基本的功能：为接收和发送设置缓冲, 提供接收和发送的实现, 和存放对端的地址信息，这些是每个连接最基本的数据和功能; 
            
  它的业务处理逻辑只是一个pingpong， 收到包原样返回。
            
  接口说明:
     virtual int register_events(int efid, std::vector<int>& cfds);
     
     事件注册接口, 由epollreactor来调用, 当tcp_port新来一个连接, 创建了一个新的client的时候, epollreactor调用此client的这个接口;
     
     efid, 是epollreactor中的epoll_create, epoll_wait用的fid;
     
     cfds, 是归属于client的连接id, 注意，一个client是可以有多个连接id的, 也可以注册多个id到efid中去, 参考的例子是redis_proxy.
     
     它说明一个client连接上来后，可以再去连接服务集群里的其它服务器，client 把connected_id push_back到这里, 
     
     epoll_reactor通过这里的值, 找到此client, 进行对它的接口进行调用。
     
     virtual int process_fire_event(const epoll_event& fire_event, int *keepalive_sec)
     
     事件处理接口, 由epollreactor来调用, 当与这个client相关的fid上有事件发生时调用。 
     
     注意, epollreactor其实并不关心client注册了什么事件，也不去分析; 注册什么事件是client自主的事情, 在它的register_events函数中去注册。
     
     client注册什么事件client自己知道, epollreactor 侦测到一个事件的发生，把整个事件的信息fire_event都传给对应的client。
     
     然后只对这个process_fire_event返回值感兴趣。三种情况
            //Return CLI_OK , 成功处理完，不需要epollreactor作什么后续工作, 
            //Return CLI_ERR, 处理失败, 有可能是对端关闭了，需要epollreactor去注销这个client.
            //Return CLI_PENDING_WRITE，返回还有数据没有写完, epollreactor后面会调用client 的write_out_data去执行写操作。
            
     virtual int write_out_data(); 写数据, 这个可以在client的process_fire_event中调用, 也可以由epollreactor调用。
     
  epollreactor:这是三个类中最复杂的一个, 它对线程进行了封装。epollreactor的对外接口是一些static函数, 这些函数对各个epollreactor实体进行管理，client分配。
  
static void create_reactors(int cnt): 创建cnt个epollreactor, 每个epollreactor是一个线程, 里面的loop 循环是运行epoll_wait.

static int  get_reactors_cnt(): 取得epollreactor的个数, 也是socket的IO进程数;

static void add_tcp_port(tcp_port_ptr& new_port): 给epollreactor增加一个tcp_port, tcp_port 都是加在首个epollreactor里的, 但产生的新的client是
       均匀分配给所有的epollreactor;                 

static void run_all_reactors(): 运行所有的epollreactor, 开启线程，进入epoll_wait!

static void stop_all_reactors(): 发送停止epollreactor的命令, 开始退出epoll_wait循环, 进而退出线程;

static void wait_all_reactors(): 等待所有的epollreactor结束, 也就是join 各个线程;

static bool wait_for_stop(int wait_out_sec): 这个是等epollreactor的结束信号, 带超时，等待wait_out_sec秒;

static void set_keepalive_args(int interval_sec, int bucket_cnt): 这个是设置剔除空闲连接的参数; 后面有详细说明;

static void add_client(client_ptr &new_client): 往epollreactor里增加一个连接，这个连接必须是非阻塞的。

epollreactor的内部流程说明:

    add_tcp_port，往epollreactor里增加了tcp的服务端口，epollreacto去调用socket, bind, listen函数，并对listen_id的事件进行侦测; 

    不像其它库要往listen_id绑定一个回调函数, 有新连接请求到来时，epollreactor会去执行accept, 并去调用对应的tcp_port的new_client

    接口，产生一个新的client, 并将其纳入管理;

    add_client, 往epollreactor里增加一个client; 像实现ccpp_client那样的功能, 就需要用到这个接口。 

    另外，像一些应用场景，几个服务器间互连互通，也要用到这个接口。

    在anet.c中有几个connect函数可供调用，产生新的连接， 如: anet_tcp_noblock_best_bind_connect;

    是否已经连接成功，需要用anet_check_noblock_connect去检测，有两个时机，在client的register_events和process_fire_event接口;

    具体实现参照pp_client的实现。

    run_all_reactors, 启动线程, 运行epoll_wait的loop--_work_loop, 它处理的事情有:

    _add_all_client: 添加所有的client, add_client()接口添加的client, 是添加在一个临时的数组里, 不是直接存放client的map--_cl_map, 

    这样子的设计主要是考虑map的线程安全和性能. 其一: _work_loop中, 对_cl_map有一个查找client过程, 如果这个过程加互斥锁则影响效率,

    所以add_client()接口只能将client的信息加在一个临时的地方, 在恰当的时候再加进去。

    epoll_wait(): 取得被触发的事件, 这些事件有两种来源, 其一来自于client注册的fid, 根据事件的fid, 去查找相应的client, 调用client的

    process_fire_event接口。 其二来自于自身的tcp_port,  根据事件的fid, 查找到相应的tcp_port, 产生新的client. add_tcp_port()的时候

    只将listen_id分配给首个epollreactor, 所以_port_map的线程安全不成问题。

    write_out_data:对哪些未完成写操作的client(在 process_fire_event 返回CLI_PENDING_WRITE), 调用此接口, 往socket写数据。若此时

    write_out_data仍然返回CLI_PENDING_WRITE(这种情形发生在写缓冲满时), 则会在下一次的循环中继续尝试去写。

    _remove_dead_client:清除空闲连接。process_fire_event中返回alive_sec, 此值代表了这个client能知epollreactor, 如果没有事件在alive_sec

    中再次发生，则可以清除掉自己。是否执行这个清除功能, 还取决于_timer_interval_sec这个值，如果此值小于零, 则不进行空闲连接剔除。

    剔除的算法, 简单说明如下: set_keepalive_args(int interval_sec, int bucket_cnt) 这个接口, 指定了剔除规则的参数。

    _timer_interval_sec = interval_sec, 小于零则不作剔除;

    interval_sec * bucket_cnt 得到一个秒数, 它的意思是一个倒计时(为方便描述, 记为TIMER_1), 一个client在TIMER_1就要超时，成为空闲连接

    被剔除。epollreactor保存client的超时信息在两个map里, _spare_wheel: 保存距离超时还比较远(>= TIMER_1)的client, _in_use_wheel 保存距离超时还比较远

    距离超时比较近(< TIMER_1)的client.  TIMER_1 又分成bucket_cnt等份, 按interval_sec秒一次执行清除工作, 所以interval_sec这个也是剔除空闲

    连接的精度。 这样, _spare_wheel是每隔TIMER_1 轮询一遍, 将超时时间小于TIMER_1 的连接放入_in_use_wheel，_in_use_wheel又是interval_sec

    秒一次的轮询, 删除到期的空闲连接。 _spare_wheel 的数据比较多, 轮询的次数就少, _in_use_wheel数据相对比较少, 轮询多几次也没关系。

    比如一种业务(通常对应一个port), 它的空闲超时设置为20分钟。而通常情况下，15分钟没有事件的连接, 90%的情况也就会到达20分钟超时了，这样我们就可以

    设置TIMER_1等于5分钟--300秒, 再去考虑它的剔除精度, 5秒还是10秒，精度细就轮询多, 精度轮询少。 利用出现超时的概率来达到优化的目的。
    
编译说明:
需要用到两个库, -lprotobuf -ltcmalloc, 编译前请先安装.

编译ccpp_client, 需要定义宏CCSERVER_PP_CLIENT, 用这行

CPPFLAGS = -g -O3 -Wall -DUSE_TCMALLOC -std=c++11 -lz -lprotobuf -ltcmalloc -DCCSERVER_PP_CLIENT

编译ccserver, 则用这行

CPPFLAGS = -g -O3 -Wall -DUSE_TCMALLOC -std=c++11 -lz -lprotobuf -ltcmalloc    

自行修改makefle,

 




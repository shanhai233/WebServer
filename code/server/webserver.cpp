#include "../config/config.h"
using namespace std;

int WebServer::m_pipefd[2] = {0};

WebServer::WebServer(int port, int trigMode, int timeoutMS, bool OptLinger, int actorModel,
                     int sqlPort, const char* sqlUser, const  char* sqlPwd,
                     const char* dbName, int connPoolNum, int threadNum,
                     bool openLog, int logLevel, int logQueSize): timer_(new HeapTimer()), 
                     epoller_(new Epoller())

{
    port_ = port;
    openLinger_ = OptLinger;
    timeoutMS_ = timeoutMS;
    threadNum_ = threadNum;
    connPoolNum_ = connPoolNum;
    openLog_ = openLog;
    logLevel_ = logLevel;
    logQueSize_ = logQueSize;
    trigMode_ = trigMode;
    sqlUser_ = sqlUser;
    sqlPwd_ = sqlPwd;
    dbName_ = dbName;
    sqlPort_ = sqlPort;
    isClose_ = false;
    actorModel_ = actorModel;

    srcDir_ = getcwd(nullptr, 256); // 获取当前的工作路径
    assert(srcDir_);
    char dir[11] = "/resources";
    strncat(srcDir_, dir, strlen(dir) + 1);    // 拼接资源路径
    
    HttpConnect::userCnt = 0;// 当前所有连接数
    HttpConnect::srcDir = srcDir_;
}

WebServer::~WebServer() {
    close(listenFd_);
    isClose_ = true;
    free(srcDir_);

    SqlConnPool::instance()->destroy();
}

// 初始化日志信息
void WebServer::LogWrite()
{
    if(openLog_) {
        Log::Instance()->init(logLevel_, "./log", ".log", logQueSize_);
    }
}

// 初始化线程池
void WebServer::ThreadPool_()
{
    ThreadPool::instance()->init(threadNum_, connPoolNum_);
}

// 初始化数据库连接池
void WebServer::SqlPool_()
{
    SqlConnPool::instance()->init("localhost", sqlPort_, sqlUser_, sqlPwd_, dbName_, connPoolNum_);
}

// 设置监听的文件描述符和通信的文件描述符的模式
void WebServer::InitEventMode_() {
    listenEvent_ = EPOLLRDHUP; //监听描述符的事件模式，检测文件描述符有没有正常关闭  EPOLLRDHUP:仅表示对方关闭了它们的连接，或仅关闭了一半的连接
    connEvent_ = EPOLLONESHOT | EPOLLRDHUP;  //EPOLLONESHOT：一个socket连接在任一时刻都只被一个线程处理
    switch (trigMode_)
    {
    case 0:
        break; //0：LT + LT  不设置，默认LT
    case 1:
        connEvent_ |= EPOLLET;    //1：LT + ET 
        break;
    case 2:
        listenEvent_ |= EPOLLET;  //2：ET + LT
        break;
    case 3:
        listenEvent_ |= EPOLLET;  //3：ET + ET
        connEvent_ |= EPOLLET;
        break;
    default:
        listenEvent_ |= EPOLLET;
        connEvent_ |= EPOLLET;
        break;
    }
    HttpConnect::isET = (connEvent_ & EPOLLET);
}

/* Create listenFd */
bool WebServer::InitSocket_() {

    //优雅关闭设置
    struct linger optLinger = { 0 };
    if(openLinger_) {
        /* 优雅关闭: 直到所剩数据发送完毕或超时 
        三种断开方式：1. l_onoff = 0; l_linger忽略   ：  close()立刻返回，底层会将未发送完的数据发送完成后再释放资源，即优雅退出。
                     2. l_onoff != 0; l_linger = 0  ：  close()立刻返回，但不会发送未发送完成的数据，而是通过一个REST包强制的关闭socket描述符，即强制退出。
                     3.  l_onoff != 0; l_linger > 0 :   close()不会立刻返回，内核会延迟一段时间，这个时间就由l_linger的值来决定。如果超时时间到达之前，发送完未发送的数据(包括FIN包)并得到另一端的确认，close()会返回正确，socket描述符优雅性退出。
                                                        否则，close()会直接返回错误值，未发送数据丢失，socket描述符被强制性退出。需要注意的时，如果socket描述符被设置为非堵塞型，则close()会直接返回值。
        */
        optLinger.l_onoff = 1;
        optLinger.l_linger = 1;
    }

    //创建监听socket (TCP)
    listenFd_ = socket(AF_INET, SOCK_STREAM, 0);
    if(listenFd_ < 0) {
        LOG_ERROR("Create socket error!", port_);
        LOG_ERROR("========== Server init error!==========");
        return false;
    }

    int ret;

    // 优雅关闭: SO_LINGER：若有数据待发则延迟关闭。
    ret = setsockopt(listenFd_, SOL_SOCKET, SO_LINGER, &optLinger, sizeof(optLinger));
    if(ret < 0) {
        close(listenFd_);
        LOG_ERROR("Init linger error!", port_);
        LOG_ERROR("========== Server init error!==========");
        return false;
    }
    
    // 端口复用
    int optval = 1;
    ret = setsockopt(listenFd_, SOL_SOCKET, SO_REUSEADDR, (const void*)&optval, sizeof(int));
    if(ret < 0) {
        close(listenFd_);
        LOG_ERROR("set socket setsockopt error !");
        LOG_ERROR("========== Server init error!==========");
        return false;
    }

    // 绑定服务器ip和端口地址
    if(port_ > 65535 || port_ < 1024) {
        LOG_ERROR("Port:%d error!",  port_);
        LOG_ERROR("========== Server init error!==========");
        return false;
    }
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port_);
    ret = bind(listenFd_, (struct sockaddr *)&addr, sizeof(addr));
    if(ret < 0) {
        close(listenFd_);
        LOG_ERROR("Bind Port:%d error!", port_);
        LOG_ERROR("========== Server init error!==========");
        return false;
    }

    //监听
    ret = listen(listenFd_, 6);
    if(ret < 0) {
        close(listenFd_);
        LOG_ERROR("Listen port:%d error!", port_);
        LOG_ERROR("========== Server init error!==========");
        return false;
    }

    //将监听的文件描述符相关的检测信息添加到epoll实例中
    ret = epoller_->AddFd(listenFd_, listenEvent_ | EPOLLIN);
    if(ret == 0) {
        close(listenFd_);
        LOG_ERROR("Add listen error!");
        LOG_ERROR("========== Server init error!==========");
        return false;
    }
    SetFdNonblock(listenFd_);
    if(openLog_)
    {
        LOG_INFO("========== Server init ==========");
        LOG_INFO("Port:%d, OpenLinger: %s", port_, openLinger_? "true":"false");
        LOG_INFO("Listen Mode: %s, OpenConn Mode: %s",
                                (listenEvent_ & EPOLLET ? "ET": "LT"),
                                (connEvent_ & EPOLLET ? "ET": "LT"));
        LOG_INFO("LogSys level: %d", logLevel_);
        LOG_INFO("srcDir: %s", HttpConnect::srcDir);
        LOG_INFO("SqlConnPool num: %d, ThreadPool num: %d", connPoolNum_, threadNum_);
        LOG_INFO("Server port:%d", port_);
    }

    //创建管道套接字
    ret = socketpair(PF_UNIX, SOCK_STREAM, 0, m_pipefd);
    assert(ret != -1);

    //设置管道写端为非阻塞
    SetFdNonblock(m_pipefd[1]);
    //设置管道读端为ET非阻塞
    ret = epoller_->AddFd(m_pipefd[0], EPOLLIN | EPOLLRDHUP);
    if(ret == 0) {
        close(m_pipefd[0]);
        LOG_ERROR("Add m_pipefd[0] error!");
        LOG_ERROR("========== Server init error!==========");
        return false;
    }
    SetFdNonblock(m_pipefd[0]);
    //传递给主循环的信号值，这里只关注SIGALRM和SIGTERM
    AddSig(SIGPIPE, SIG_IGN);
    AddSig(SIGALRM, Sig_Handler, false);
    AddSig(SIGTERM, Sig_Handler, false);
    //每隔timeoutMS时间触发SIGALRM信号
    alarm(timeoutMS_);

    return true;
}

//添加信号
void WebServer::AddSig(int sig, void(handler)(int), bool restart)
{
    //创建sigaction结构体变量
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));

    //信号处理函数中仅仅发送信号值，不做对应逻辑处理
    sa.sa_handler = handler;
    if (restart)
        sa.sa_flags |= SA_RESTART;
    
    //将所有信号添加到信号集中
    sigfillset(&sa.sa_mask);

    //执行sigaction函数
    assert(sigaction(sig, &sa, NULL) != -1);
}

// 启动服务器
void WebServer::EventLoop() {
    bool timeout = false;
    //int timeMS = -1;  /* epoll wait timeout == -1 无事件将阻塞 */
    if(!isClose_) { LOG_INFO("========== Server start =========="); }
    while(!isClose_) {

        int eventCnt = epoller_->Wait(-1);
        if (eventCnt < 0 && errno != EINTR)
        {
            LOG_ERROR("%s", "epoll failure");
            break;
        }
        // 循环处理每一个事件
        for(int i = 0; i < eventCnt; i++) {
            /* 处理事件 */
            int fd = epoller_->GetEventFd(i);   // 获取事件对应的fd
            uint32_t events = epoller_->GetEvents(i);   // 获取事件的类型
            
            // 监听的文件描述符有事件，说明有新的连接进来
            if(fd == listenFd_) {
                DealListen_();  // 处理监听的操作，接受客户端连接
            }
            
            // 错误的一些情况
            else if(events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) {
                assert(users_.count(fd) > 0);
                CloseConn_(&users_[fd]);    // 关闭连接
            }

            //处理信号  //管道读端对应文件描述符发生读事件
            else if ((listenFd_ == m_pipefd[0]) && (events & EPOLLIN))
            {
                bool flag = DealwithSignal(timeout, isClose_);
                if (false == flag)
                    LOG_ERROR("%s", "dealclientdata failure");
            }

            // 有数据到达
            else if(events & EPOLLIN) {
                assert(users_.count(fd) > 0);
                DealRead_(&users_[fd]); // 处理读操作
            }
            
            // 可以发送数据
            else if(events & EPOLLOUT) {
                assert(users_.count(fd) > 0);
                DealWrite_(&users_[fd]);    // 处理写操作
            } else {
                LOG_ERROR("Unexpected event");
            }
        }
        if (timeout)
        {
            Timer_Handler();
            LOG_INFO("%s", "timer tick");
            timeout = false;
        }
    }
}

//定时处理
void WebServer::Timer_Handler()
{
    int timeMS = timer_->GetNextTick();
    alarm(timeMS);
}

//信号处理
bool WebServer::DealwithSignal(bool &timeout, bool &stop_server)
{
    int ret = 0;
    int sig;
    char signals[1024];
    //正常情况下，这里的ret返回值总是1，只有14和15两个ASCII码对应的字符
    ret = recv(m_pipefd[0], signals, sizeof(signals), 0);
    if (ret == -1)
    {
        return false;
    }
    else if (ret == 0)
    {
        return false;
    }
    else
    {
        //处理信号值对应的逻辑
        for (int i = 0; i < ret; ++i)
        {
            //这里面明明是字符
            switch (signals[i])
            {
            //这里是整型
            case SIGALRM:
            {
                timeout = true;
                break;
            }
            case SIGTERM:
            {
                stop_server = true;
                break;
            }
            }
        }
    }
    return true;
}

// 发送错误提示信息
void WebServer::SendError_(int fd, const char*info) {
    assert(fd > 0);
    int ret = send(fd, info, strlen(info), 0);
    if(ret < 0) {
        LOG_WARN("send error to client[%d] error!", fd);
    }
    close(fd);
}

// 关闭连接（从epoll中删除，解除响应对象中的内存映射，用户数递减，关闭文件描述符）
void WebServer::CloseConn_(HttpConnect* client) {
    assert(client);
    LOG_INFO("Client[%d] quit!", client->getfd());
    epoller_->DelFd(client->getfd());
    client->closeConnect();
}

// 添加客户端
void WebServer::AddClient_(int fd, sockaddr_in addr) {
    assert(fd > 0);
    users_[fd].init(fd, addr);
    if(timeoutMS_ > 0) {
        // 添加到定时器对象中，当检测到超时时执行CloseConn_函数进行关闭连接
        timer_->add(fd, timeoutMS_, std::bind(&WebServer::CloseConn_, this, &users_[fd]));
    }
    // 添加到epoll中进行管理
    epoller_->AddFd(fd, EPOLLIN | connEvent_);
    // 设置文件描述符非阻塞
    SetFdNonblock(fd);
    LOG_INFO("Client[%d] in!", users_[fd].getfd());
}

// 处理监听事件
void WebServer::DealListen_() {
    struct sockaddr_in addr; // 保存连接的客户端的信息
    socklen_t len = sizeof(addr);
    // 如果监听文件描述符设置的是 ET模式，则需要循环把所有连接处理了
    do {
        int fd = accept(listenFd_, (struct sockaddr *)&addr, &len);
        if(fd <= 0) { return;}
        else if(HttpConnect::userCnt >= MAX_FD) {
            SendError_(fd, "Server busy!");
            LOG_WARN("Clients is full!");
            return;
        }
        AddClient_(fd, addr);   // 添加客户端
    } while(listenEvent_ & EPOLLET);
}

// 处理读
void WebServer::DealRead_(HttpConnect* client) {
    assert(client);
    if(actorModel_ == 0) //Reactor
    {
        // 延长这个客户端的超时时间
        ExtentTime_(client);  
        // 加入到队列中等待线程池中的线程处理（读取数据）
        ThreadPool::instance()->addTask(std::bind(&WebServer::OnRead_, this, client));
    }
    else //Proactor
    {
        int ret = -1;
        int readErrno = 0;
        ret = client->read(&readErrno); // 读取客户端的数据
        if(ret <= 0 && readErrno != EAGAIN) 
        {
            CloseConn_(client);
            return;
        }
        ExtentTime_(client); 
        ThreadPool::instance()->addTask(std::bind(&WebServer::OnProcess, this, client));
    }
}

// 处理写
void WebServer::DealWrite_(HttpConnect* client) {
    assert(client);
    if(actorModel_ == 0) //Reactor
    {
        ExtentTime_(client);// 延长这个客户端的超时时间
        // 加入到队列中等待线程池中的线程处理（写数据）
        ThreadPool::instance()->addTask(std::bind(&WebServer::OnWrite_, this, client));
    }
    else
    {
        int ret = -1;
        int writeErrno = 0;
        ret = client->write(&writeErrno);   // 写数据
        if(ret < 0) {
            if(writeErrno == EAGAIN) {
                /* 继续传输 */
                epoller_->ModFd(client->getfd(), connEvent_ | EPOLLOUT);
                return;
            }
        }

        ExtentTime_(client);// 延长这个客户端的超时时间
        // 加入到队列中等待线程池中的线程处理（写数据）
        ThreadPool::instance()->addTask(std::bind(&WebServer::OnProcess_Write_, this, client));
    }
    
}

// 延长客户端的超时时间
void WebServer::ExtentTime_(HttpConnect* client) {
    assert(client);
    if(timeoutMS_ > 0) { timer_->adjust(client->getfd(), timeoutMS_); }
}

// 这个方法是在子线程中执行的（读取数据）
void WebServer::OnRead_(HttpConnect* client) {
    assert(client);
    int ret = -1;
    int readErrno = 0;
    ret = client->read(&readErrno); // 读取客户端的数据
    if(ret <= 0 && readErrno != EAGAIN) {
        CloseConn_(client);
        return;
    }

    // 业务逻辑的处理
    OnProcess(client);
}

// 业务逻辑的处理
void WebServer::OnProcess(HttpConnect* client) {
    if(client->process()) {
        epoller_->ModFd(client->getfd(), connEvent_ | EPOLLOUT); //修改为监听可写事件
    } else {
        epoller_->ModFd(client->getfd(), connEvent_ | EPOLLIN);
    }
}

//处理Proactor模式下的写操作
void WebServer::OnProcess_Write_(HttpConnect* client) {
    // 如果将要写的字节等于0，说明写完了，判断是否要保持连接，保持连接继续去处理
        if(client->toWriteBytes() == 0) {
            /* 传输完成 */
            if(client->isKeepAlive()) {
                OnProcess(client);
                return;
            }
        }
        CloseConn_(client);
}

// 写数据
void WebServer::OnWrite_(HttpConnect* client) {
    assert(client);
    int ret = -1;
    int writeErrno = 0;
    ret = client->write(&writeErrno);   // 写数据

    // 如果将要写的字节等于0，说明写完了，判断是否要保持连接，保持连接继续去处理
    if(client->toWriteBytes() == 0) {
        /* 传输完成 */
        if(client->isKeepAlive()) {
            OnProcess(client);
            return;
        }
    }
    else if(ret < 0) {
        if(writeErrno == EAGAIN) {
            /* 继续传输 */
            epoller_->ModFd(client->getfd(), connEvent_ | EPOLLOUT);
            return;
        }
    }
    CloseConn_(client);
}

// 设置文件描述符非阻塞
int WebServer::SetFdNonblock(int fd) {
    assert(fd > 0);
    // int flag = fcntl(fd, F_GETFD, 0);
    // flag = flag  | O_NONBLOCK;
    // // flag  |= O_NONBLOCK;
    // fcntl(fd, F_SETFL, flag);
    return fcntl(fd, F_SETFL, fcntl(fd, F_GETFD, 0) | O_NONBLOCK);
}

//信号处理函数
void Sig_Handler(int sig)
{
    //为保证函数的可重入性，保留原来的errno
    //可重入性表示中断后再次进入该函数，环境变量与之前相同，不会丢失数据
    int save_errno = errno;
    int msg = sig;

    //将信号值从管道写端写入，传输字符类型，而非整型
    send(WebServer::m_pipefd[1], (char *)&msg, 1, 0);

    //将原来的errno赋值为当前的errno
    errno = save_errno;
}

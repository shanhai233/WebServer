#ifndef WEBSERVER_H
#define WEBSERVER_H

#include "../config/config.h"

class WebServer {
public:
    WebServer(
        int port, int trigMode, int timeoutMS, bool OptLinger, int actorModel,
        int sqlPort, const char* sqlUser, const  char* sqlPwd, 
        const char* dbName, int connPoolNum, int threadNum,
        bool openLog, int logLevel, int logQueSize);
    
    WebServer();

    // void init(int port, int trigMode, int timeoutMS, bool OptLinger,
    //                  int sqlPort, const char* sqlUser, const  char* sqlPwd,
    //                  const char* dbName, int connPoolNum, int threadNum,
    //                  bool openLog, int logLevel, int logQueSize);

    ~WebServer();
    void EventLoop();

public:
    bool InitSocket_(); 
    void InitEventMode_();
    void AddClient_(int fd, sockaddr_in addr);
  
    void DealListen_();
    void DealWrite_(HttpConnect* client);
    void DealRead_(HttpConnect* client);

    void SendError_(int fd, const char*info);
    void ExtentTime_(HttpConnect* client);
    void CloseConn_(HttpConnect* client);

    void OnRead_(HttpConnect* client);
    void OnWrite_(HttpConnect* client);
    void OnProcess(HttpConnect* client);
    void OnProcess_Write_(HttpConnect* client);
    void OnProcess_Read_(HttpConnect* client);

    void LogWrite();
    void SqlPool_();
    void ThreadPool_();

    static const int MAX_FD = 65536;    // 最大的文件描述符的个数

    static int SetFdNonblock(int fd);   // 设置文件描述符非阻塞

    int port_;          // 端口
    bool openLinger_;   // 是否打开优雅关闭
    int timeoutMS_;  /* 毫秒MS */
    bool isClose_;  // 是否关闭
    int listenFd_;  // 监听的文件描述符
    char* srcDir_;  // 资源的目录
    int actorModel_;
    
    uint32_t listenEvent_;  // 监听的文件描述符的事件
    uint32_t connEvent_;    // 连接的文件描述符的事件

    int connPoolNum_;
    int threadNum_;
    bool openLog_;
    int logLevel_;
    int logQueSize_;
    int trigMode_;
    const char* sqlUser_ ;
    const  char* sqlPwd_;
    const char* dbName_;
    int sqlPort_;

    //int m_pipefd[2];
    static int m_pipefd[2];
    
    
    void AddSig(int sig, void(handler)(int), bool restart = true);//设置信号函数
    void Timer_Handler();//定时处理任务，重新定时以不断触发SIGALRM信号
    bool DealwithSignal(bool& timeout, bool& stop_server);
    
    //防止内存泄漏的智能指针unique_ptr
    std::unique_ptr<HeapTimer> timer_;  // 定时器   
    //std::unique_ptr<ThreadPool> threadpool_;    // 线程池
    std::unique_ptr<Epoller> epoller_;      // epoll对象

    // HeapTimer* timer_;  // 定时器   
    // ThreadPool* threadpool_;    // 线程池
    // Epoller* epoller_;      // epoll对象

    std::unordered_map<int, HttpConnect> users_;   // 保存的是客户端连接的信息
};

void Sig_Handler(int sig);//信号处理函数

#endif //WEBSERVER_H
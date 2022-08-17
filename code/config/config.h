#ifndef CONFIG_H
#define CONFIG_H

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <cassert>
#include <sys/epoll.h>
#include <netinet/tcp.h> 
#include <unordered_map>      
#include <assert.h>
#include <signal.h>


#include "../epoll/epoller.h"
#include "../log/log.h"
#include "../timer/heaptimer.h"
#include "../sqlconnpool/sql_conn_pool.h"
#include "../threadpool/threadpool.h"
// #include "../pool/sqlconnRAII.h"
#include "../http/httpconn.h"
#include "../server/webserver.h"

using namespace std;

class Config
{
public:
    Config();
    ~Config(){};

    //void parse_arg(int argc, char*argv[]);

    //端口号
    int PORT;

    //数据库端口号
    int MYSQLPORT;

    //日志写入方式
    int LOGWrite;

    //日志写入方式
    int LOGLevel;

    //触发组合模式
    int TRIGMode;

    //listenfd触发模式
    int LISTENTrigmode;

    //connfd触发模式
    int CONNTrigmode;

    //优雅关闭链接
    bool OPT_LINGER;

    //数据库连接池数量
    int SqlConn_Num;

    //线程池内的线程数量
    int ThreadConn_Num;

    //是否打开日志
    int OpenLog;

    //并发模型选择
    int ActorModel;

    //定时器时延
    int timeout;

    //日志异步队列容量
    int LOG1Count;
};

#endif
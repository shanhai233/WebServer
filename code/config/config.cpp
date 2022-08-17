#include "config.h"

Config::Config(){
    //端口号,默认9001
    PORT = 9001;

    //日志写入方式，默认同步
    LOGWrite = 0;

    //日志等级    0：DEBUG     1：INFO     2：WARN     3：ERROR
    LOGLevel = 1;

    //日志异步队列容量
    LOG1Count = 1024;

    //打开日志,默认不打开
    OpenLog = 0;

    //触发组合模式,默认3  ( listenfd + connfd ) 0: LT + LT  1：LT + ET  2：ET + LT  3：ET + ET
    TRIGMode = 3;

    //listenfd触发模式，默认ET
    LISTENTrigmode = 1;

    //connfd触发模式，默认ET
    CONNTrigmode = 1;

    //优雅关闭链接，默认不使用
    OPT_LINGER = false;

    //数据库连接池数量,默认8
    SqlConn_Num = 8;

    //线程池内的线程数量,默认8
    ThreadConn_Num = 8;

    //并发模型,默认是Reactor
    ActorModel = 0;

    //定时器时延毫秒，默认1分钟
    timeout = 60000;

    //数据库端口号，默认3306
    MYSQLPORT = 3306;
}

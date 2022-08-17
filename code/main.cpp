#include "./config/config.h"

int main(int argc, char *argv[])
{
    //需要修改的数据库信息,登录名,密码,库名
    const char*  user = "root";
    const char*  passwd = "root";
    const char*  databasename = "yourdb";

    //命令行解析
    Config config;
    //config.parse_arg(argc, argv);

    WebServer server(
        config.PORT, config.TRIGMode, config.timeout, config.OPT_LINGER, config.ActorModel,      /* 端口 ET模式 timeoutMs 优雅退出  */
        config.MYSQLPORT, user, passwd, databasename, /* Mysql配置：端口号、用户名、密码、数据库的名称 */
        config.SqlConn_Num, config.ThreadConn_Num, config.OpenLog, config.LOGLevel, config.LOG1Count);   /* 连接池数量 线程池数量 日志开关 日志等级 日志异步队列容量 */  


    //日志
    server.LogWrite();

    //线程池
    server.ThreadPool_();

    //数据库
    server.SqlPool_();
    
    //触发模式
    server.InitEventMode_();
    
    //socket监听
    if(!server.InitSocket_()) 
    { 
        server.isClose_ = true;
    }
    
    //运行
    server.EventLoop();

    return 0;
}
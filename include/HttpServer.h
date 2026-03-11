#pragma once
#include "EventLoop.h"
#include <string>

class HttpServer {
public:
    HttpServer(int port);
    ~HttpServer();

    void start(); // 启动服务器

private:
    int port_;
    int listen_fd_;
    EventLoop loop_; // HttpServer 拥有一个事件循环引擎

    void initSocket(); // 封装原来的 socket/bind/listen
    
    // 核心业务函数：当 EventLoop 收到数据时，会回调这个函数
    void handleMessage(int client_fd, const std::string& raw_data);
};
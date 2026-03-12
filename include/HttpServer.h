#pragma once
#include "EventLoop.h"
#include <string>
#include <unordered_map>

// 【新增】用来记录每个客户端正在下载的文件状态
struct FileState {
    int file_fd;      // 本地磁盘上的文件描述符
    off_t offset;     // 当前已经发送了多少字节（进度条）
    off_t file_size;  // 文件的总大小
};

class HttpServer {
public:
    HttpServer(int port);
    ~HttpServer();

    void start();

private:
    int port_;
    int listen_fd_;
    EventLoop loop_;

    // 【新增】核心记账本：<客户端 fd, 对应的文件发送状态>
    std::unordered_map<int, FileState> file_states_;

    void initSocket();
    void handleMessage(int client_fd, const std::string& raw_data);
    
    // 【新增】处理内核可写事件（继续发送剩余文件）
    void handleWrite(int client_fd);
};
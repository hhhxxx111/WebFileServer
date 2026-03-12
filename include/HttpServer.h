#pragma once
#include "EventLoop.h"
#include "ThreadPool.h" // 【新增】引入打工人团队
#include <string>
#include <unordered_map>
#include <mutex>        // 【新增】引入互斥锁

struct FileState {
    int file_fd;
    off_t offset;
    off_t file_size;
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
    
    // 【新增】实例化一个线程池
    ThreadPool pool_;

    std::unordered_map<int, FileState> file_states_;
    // 【新增】保护 file_states_ 记账本的专属密码锁！
    std::mutex state_mutex_; 

    void initSocket();
    void handleMessage(int client_fd, const std::string& raw_data);
    void handleWrite(int client_fd);
};
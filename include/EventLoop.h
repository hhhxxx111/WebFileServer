#pragma once
#include <sys/epoll.h>
#include <vector>
#include <string>
#include <functional>

using MessageCallback = std::function<void(int, const std::string&)>;
// 新增：专门处理“缓冲区空闲，可以继续写了”的回调
using WriteCallback = std::function<void(int)>; 

class EventLoop {
public:
    EventLoop();
    ~EventLoop();

    EventLoop(const EventLoop&) = delete;
    EventLoop& operator=(const EventLoop&) = delete;

    void addFd(int fd, uint32_t events);
    void delFd(int fd);
    
    // 【新增】修改已存在 fd 的监听事件（比如从 EPOLLIN 改为 EPOLLIN | EPOLLOUT）
    void modifyFd(int fd, uint32_t events); 

    void loop(int listen_fd);

    void setMessageCallback(MessageCallback cb) { onMessage_ = cb; }
    // 【新增】注册写回调
    void setWriteCallback(WriteCallback cb) { onWrite_ = cb; }

private:
    int epoll_fd_;
    std::vector<struct epoll_event> active_events_;
    MessageCallback onMessage_;
    WriteCallback onWrite_; // 【新增】保存上层注册的写回调函数
};
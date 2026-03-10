#pragma once
#include <sys/epoll.h>
#include <vector>

class EventLoop {
public:
    EventLoop();
    ~EventLoop();

    // 禁用拷贝构造和赋值操作（资源独占）
    EventLoop(const EventLoop&) = delete;
    EventLoop& operator=(const EventLoop&) = delete;

    // 将 fd 添加到 epoll 中监听指定的事件（比如 EPOLLIN 代表可读）
    void addFd(int fd, uint32_t events);
    
    // 从 epoll 中移除对某个 fd 的监听
    void delFd(int fd);
    
    // 开启核心的事件循环（死循环，不断等待事件发生）
    void loop();

private:
    int epoll_fd_; // epoll 实例本身也是一个文件描述符
    std::vector<struct epoll_event> active_events_; // 用来接收内核返回的“活跃事件”
};
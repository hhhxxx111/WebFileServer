#include "../include/EventLoop.h"
#include <iostream>
#include <unistd.h>

// 构造函数：创建 epoll 实例
EventLoop::EventLoop() : active_events_(1024) {
    // epoll_create1(0) 是较新的 API，比老的 epoll_create 更推荐
    epoll_fd_ = epoll_create1(EPOLL_CLOEXEC);
    if (epoll_fd_ == -1) {
        std::cerr << "Fatal Error: Failed to create epoll file descriptor!" << std::endl;
        exit(EXIT_FAILURE);
    }
}

// 析构函数：关闭 epoll 句柄
EventLoop::~EventLoop() {
    close(epoll_fd_);
}

// 核心封装 1：添加/修改监听
void EventLoop::addFd(int fd, uint32_t events) {
    struct epoll_event ev;
    ev.events = events; // 关注的事件，比如 EPOLLIN (可读), EPOLLET (边缘触发)
    ev.data.fd = fd;    // 顺带把 fd 存进去，等事件触发时内核会原样还给我们，方便我们知道是谁触发了

    // EPOLL_CTL_ADD 表示添加监听
    if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, fd, &ev) == -1) {
        std::cerr << "Error: epoll_ctl add failed for fd " << fd << std::endl;
    }
}

// 核心封装 2：取消监听
void EventLoop::delFd(int fd) {
    // EPOLL_CTL_DEL 表示删除监听
    if (epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, fd, nullptr) == -1) {
        std::cerr << "Error: epoll_ctl del failed for fd " << fd << std::endl;
    }
}

// 核心引擎：死循环监听事件
void EventLoop::loop() {
    std::cout << "🔄 EventLoop is running, waiting for events..." << std::endl;

    while (true) {
        // epoll_wait 会在这里阻塞，直到有事件发生
        // 参数 -1 表示无限期等待，有事件才醒来
        int num_events = epoll_wait(epoll_fd_, active_events_.data(), active_events_.size(), -1);
        
        if (num_events == -1) {
            std::cerr << "Error: epoll_wait failed!" << std::endl;
            break;
        }
 
        // 遍历所有被触发的事件
        for (int i = 0; i < num_events; ++i) {
            int active_fd = active_events_[i].data.fd;
            uint32_t events = active_events_[i].events;

            // 这里先简单打印出来，看看是哪个 fd 发生了什么事
            std::cout << "[epoll] Event triggered on fd: " << active_fd << std::endl;
            
            // TODO: 后续我们将在这里根据 active_fd 是 listen_fd 还是 client_fd，
            // 来决定是调用 accept 还是 read/write！
        }
    }
}
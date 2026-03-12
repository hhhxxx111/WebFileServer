#include "../include/EventLoop.h"
#include <iostream>
#include <unistd.h>

#include <sys/socket.h> // 提供 accept() 和 sockaddr 结构体
#include <netinet/in.h> // 提供 sockaddr_in 结构体 (IPv4地址)
#include <arpa/inet.h>  // 提供 inet_ntoa() 函数 (将IP转换为字符串)
#include <filesystem>
#include <fstream>

#include <sys/sendfile.h> // 提供 sendfile()
#include <sys/stat.h>     // 提供 fstat() 来获取文件大小
#include <fcntl.h>        // 提供 open() 和 O_RDONLY

#include "../include/HttpContext.h"

// 辅助函数：将文件描述符设置为非阻塞
void setNonBlocking(int fd) {
    // 1. 获取该 fd 当前的属性标志位（File Status Flags）
    int flags = fcntl(fd, F_GETFL, 0);
    
    // 2. 在原有的标志位基础上，通过位或运算（|）追加 O_NONBLOCK（非阻塞）标志，然后再设置回去
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

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

// 核心封装 3：修改监听事件
void EventLoop::modifyFd(int fd, uint32_t events) {
    struct epoll_event ev;
    ev.events = events;
    ev.data.fd = fd;

    // EPOLL_CTL_MOD 表示修改
    if (epoll_ctl(epoll_fd_, EPOLL_CTL_MOD, fd, &ev) == -1) {
        std::cerr << "Error: epoll_ctl mod failed for fd " << fd << std::endl;
    }
}

// 核心引擎：死循环监听事件
void EventLoop::loop(int listen_fd) {
    std::cout << "🔄 EventLoop is running, waiting for events..." << std::endl;

    while (true) {
        int num_events = epoll_wait(epoll_fd_, active_events_.data(), active_events_.size(), -1);
        
        if (num_events == -1) {
            std::cerr << "Error: epoll_wait failed!" << std::endl;
            break;
        }

        for (int i = 0; i < num_events; ++i) {
            int active_fd = active_events_[i].data.fd;

            // 情况 1：如果是 listen_fd 触发了事件，说明有新的浏览器连接进来了！
            if (active_fd == listen_fd) {
                struct sockaddr_in client_addr;
                socklen_t client_len = sizeof(client_addr);
                
                // 接收新连接
                int client_fd = accept(listen_fd, (struct sockaddr*)&client_addr, &client_len);
                if (client_fd > 0) {
                    std::cout << "[+] New connection from: " << inet_ntoa(client_addr.sin_addr) 
                              << " (fd: " << client_fd << ")" << std::endl;
                    
                    // 将新连接也设置为非阻塞
                    setNonBlocking(client_fd);
                    
                    // 把这个新客人的 fd 也扔进 epoll 里监听它的“可读”事件
                    // EPOLLET 表示边缘触发（Edge Triggered），性能更高
                    addFd(client_fd, EPOLLIN | EPOLLET); 
                }
            } 
            // 情况 2：如果是其他 fd，说明是已经连接的浏览器给我们发 HTTP 请求了
            // 情况 2：已连接的浏览器有动静了
            else {
                uint32_t events = active_events_[i].events;

                // ==========================================
                // 场景 A：浏览器发来了数据 (EPOLLIN 可读事件)
                // ==========================================
                if (events & EPOLLIN) {
                    std::string request_data; 
                    bool client_closed = false;

                    while (true) {
                        char buffer[4096] = {0};
                        ssize_t bytes_read = read(active_fd, buffer, sizeof(buffer) - 1);

                        if (bytes_read > 0) {
                            request_data.append(buffer, bytes_read);
                        } else if (bytes_read == -1) {
                            if (errno == EAGAIN || errno == EWOULDBLOCK) break;
                            else { client_closed = true; break; }
                        } else if (bytes_read == 0) {
                            client_closed = true; break;
                        }
                    }

                    if (!client_closed && !request_data.empty()) {
                        if (onMessage_) onMessage_(active_fd, request_data);
                    }

                    if (client_closed) {
                        delFd(active_fd);
                        close(active_fd);
                    }
                }

                // ==========================================
                // 场景 B：内核发送缓冲区空出来了 (EPOLLOUT 可写事件)
                // ==========================================
                if (events & EPOLLOUT) {
                    // 呼叫上层业务：缓冲区腾出空间了，刚才没发完的文件可以接着发了！
                    if (onWrite_) {
                        onWrite_(active_fd);
                    }
                }
            }
        }
    }
}
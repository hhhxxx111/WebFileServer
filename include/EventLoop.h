#pragma once
#include <sys/epoll.h>
#include <vector>
#include <string>      // 👈 就是缺了这位大哥！
#include <functional>

// 定义回调函数类型：参数是 (触发事件的 fd, 读到的完整字符串)
using MessageCallback = std::function<void(int, const std::string&)>;

class EventLoop {
public:
    EventLoop();
    ~EventLoop();

    // 禁用拷贝构造和赋值操作
    EventLoop(const EventLoop&) = delete;
    EventLoop& operator=(const EventLoop&) = delete;

    void addFd(int fd, uint32_t events);
    void delFd(int fd);
    void loop(int listen_fd);

    // 注册回调函数的接口 (刚才报错说找不到这个成员，其实是因为上面 string 报错导致的连锁反应)
    void setMessageCallback(MessageCallback cb) { onMessage_ = cb; }

private:
    int epoll_fd_;
    std::vector<struct epoll_event> active_events_;
    MessageCallback onMessage_; // 保存上层注册进来的业务函数
};
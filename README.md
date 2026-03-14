# WebFileServer

A high-performance, concurrent web file server based on C++11 and Linux epoll. 

本项目脱离了臃肿的现代 Web 框架，从底层 Socket 编程出发，深度结合了 Reactor 并发模型、Zero-Copy 技术以及有限状态机 (FSM)，旨在以极低的内存和 CPU 开销，实现海量并发连接与大文件的极速分发。

## 核心架构与技术栈

* **并发模型 (Reactor 模式)**
  采用 Linux `epoll` I/O 多路复用技术，配置为边缘触发 (EPOLLET) 结合非阻塞 Socket。
  实现 Main-Sub Reactor 架构：主线程专注监听并分发新连接，将复杂的 HTTP 解析与磁盘 I/O 任务异步投递给工作线程池。
* **任务线程池 (Thread Pool)**
  纯手写基于 C++11 (`std::thread`, `std::mutex`, `std::condition_variable`) 的泛型任务线程池。通过闭包与任务队列解耦网络收发与业务逻辑，充分压榨多核 CPU 性能。
* **零拷贝技术 (Zero-Copy)**
  大文件传输摒弃传统的 `read/write` 用户态内存中转，直接调用系统级 API `sendfile()`，实现磁盘数据直通网卡发送缓冲区，内存占用接近于零。
* **异步发送与断点续传**
  针对非阻塞网络编程中的写拥塞问题 (`EAGAIN` / `EWOULDBLOCK`)，引入 `EPOLLOUT` 事件监听机制。结合哈希表构建发送状态机，在 TCP 缓冲区满时自动挂起，腾出空位后精准接续发送，彻底解决大文件传输中断问题。
* **HTTP 协议解析**
  构建有限状态机 (FSM) 逐行解析 HTTP 报文，精准提取请求行与头部字段，支持 Keep-Alive 长连接。结合 C++17 `<filesystem>` 库实现本地目录的动态扫描与 HTML 渲染。

## 环境要求

* 操作系统：Linux (Ubuntu / CentOS)
* 编译器：g++ (支持 C++17)
* 依赖项：`<pthread>`

## 目录结构

```text
.
├── include/
│   ├── EventLoop.h      # epoll 核心封装与事件分发
│   ├── HttpContext.h    # HTTP 报文解析状态机
│   ├── HttpServer.h     # 路由分发与发送状态簿
│   └── ThreadPool.h     # C++11 并发任务线程池
├── src/
│   ├── EventLoop.cpp
│   ├── HttpContext.cpp
│   ├── HttpServer.cpp
│   └── main.cpp         # 程序入口
├── www/                 # 静态资源与待下载文件存放目录
└── .gitignore
#include "../include/HttpServer.h"
#include "../include/HttpContext.h"
#include <iostream>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <sys/sendfile.h>
#include <sys/stat.h>
#include <filesystem>

HttpServer::HttpServer(int port) : port_(port), listen_fd_(-1) {
    initSocket();

    // 【核心解耦点】把自己的 handleMessage 函数，绑定给 EventLoop 当作回调！
    // 当 loop_ 收到数据时，就会自动触发 HttpServer::handleMessage
    loop_.setMessageCallback([this](int fd, const std::string& data) {
        this->handleMessage(fd, data);
    });

    // 【新增】把 handleWrite 绑定给 EventLoop 当作可写事件的回调
    loop_.setWriteCallback([this](int fd) {
        this->handleWrite(fd);
    });
}

HttpServer::~HttpServer() {
    if (listen_fd_ != -1) {
        close(listen_fd_);
    }
}

void HttpServer::initSocket() {
    listen_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    int optval = 1;
    setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(port_);

    bind(listen_fd_, (struct sockaddr*)&server_addr, sizeof(server_addr));
    listen(listen_fd_, SOMAXCONN);
    std::cout << "🚀 HttpServer init success, listening on port " << port_ << "..." << std::endl;
}

void HttpServer::start() {
    loop_.addFd(listen_fd_, EPOLLIN);
    loop_.loop(listen_fd_); // 启动底层引擎
}

// 纯粹的业务处理中心
void HttpServer::handleMessage(int active_fd, const std::string& raw_data) {
    HttpContext context;
    if (!context.parse(raw_data)) {
        std::string bad_req = "HTTP/1.1 400 Bad Request\r\nContent-Length: 0\r\n\r\n";
        write(active_fd, bad_req.c_str(), bad_req.length());
        loop_.delFd(active_fd);
        close(active_fd);
        return;
    }

    const HttpRequest& req = context.getRequest();
    std::string response;

    // 路由分发
    if (req.path == "/") {
        std::string html = "<html><head><meta charset='utf-8'><title>C++ WebFileServer</title></head><body><h1>📁 文件列表</h1><ul>";
        for (const auto& entry : std::filesystem::directory_iterator("./www")) {
            std::string filename = entry.path().filename().string();
            html += "<li><a href=\"/download/" + filename + "\">" + filename + "</a></li>";
        }
        html += "</ul></body></html>";
        response = "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\nContent-Length: " + std::to_string(html.length()) + "\r\n\r\n" + html;
        write(active_fd, response.c_str(), response.length());
    } 
    else if (req.path.find("/download/") == 0) {
        std::string filename = req.path.substr(10); 
        std::string filepath = "./www/" + filename;
        int file_fd = open(filepath.c_str(), O_RDONLY);
        
        if (file_fd < 0) {
            response = "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\n\r\n";
            write(active_fd, response.c_str(), response.length());
        } else {
            struct stat file_stat;
            fstat(file_fd, &file_stat);
            off_t file_size = file_stat.st_size;

            std::string header = "HTTP/1.1 200 OK\r\nContent-Type: application/octet-stream\r\n"
                                 "Content-Disposition: attachment; filename=\"" + filename + "\"\r\n"
                                 "Content-Length: " + std::to_string(file_size) + "\r\n\r\n";
            write(active_fd, header.c_str(), header.length());

            off_t offset = 0; 
            bool send_complete = false;

            while (offset < file_size) {
                ssize_t sent = sendfile(active_fd, file_fd, &offset, file_size - offset);
                
                if (sent < 0) {
                    if (errno == EAGAIN || errno == EWOULDBLOCK) {
                        // 【核心魔法】网卡缓冲区满了！不报错，记下进度，修改监听状态！
                        std::cout << "[*] 发送缓冲区满，保存进度，等待 EPOLLOUT... (fd: " << active_fd << ")" << std::endl;
                        
                        // 1. 记账
                        file_states_[active_fd] = {file_fd, offset, file_size};
                        
                        // 2. 告诉 epoll：以后这个 active_fd 只要有空位能写数据了，立刻叫醒我！
                        loop_.modifyFd(active_fd, EPOLLIN | EPOLLOUT | EPOLLET);
                        break; // 优雅跳出，不阻塞线程
                    } else {
                        std::cerr << "[-] sendfile 发生严重错误!" << std::endl;
                        close(file_fd);
                        break;
                    }
                } else if (sent == 0) {
                    break; // EOF
                }
            }

            // 如果一次性发完了（比如文件很小，或者网速极快）
            if (offset >= file_size) {
                std::cout << "✅ 文件传输直接完成: " << filename << std::endl;
                close(file_fd);
            }
        }
    } else {
        response = "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\n\r\n";
        write(active_fd, response.c_str(), response.length());
    }
}

// 【全新方法】处理异步可写事件
void HttpServer::handleWrite(int client_fd) {
    // 1. 翻开记账本，看看这个 fd 有没有还没发完的文件
    if (file_states_.find(client_fd) == file_states_.end()) {
        return; // 没有挂起的任务，直接返回
    }

    FileState& state = file_states_[client_fd];
    std::cout << "[~] 缓冲区已腾出空间，继续发送... (fd: " << client_fd << ", 进度: " << state.offset << "/" << state.file_size << ")" << std::endl;

    // 2. 顺着上次的 offset 继续发送
    while (state.offset < state.file_size) {
        ssize_t sent = sendfile(client_fd, state.file_fd, &state.offset, state.file_size - state.offset);
        
        if (sent < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // 如果又发满了，没关系，直接 return 回去等下一次 EPOLLOUT
                return; 
            } else {
                std::cerr << "[-] 异步 sendfile 期间发生错误" << std::endl;
                close(state.file_fd);
                file_states_.erase(client_fd);
                // 恢复为只监听读事件，不再监听写事件
                loop_.modifyFd(client_fd, EPOLLIN | EPOLLET);
                return;
            }
        }
    }

    // 3. 终于全部发完了！
    if (state.offset >= state.file_size) {
        std::cout << "✅ 异步文件传输完美结束！(fd: " << client_fd << ")" << std::endl;
        
        // 清理战场
        close(state.file_fd);
        file_states_.erase(client_fd);
        
        // 告诉 epoll：不用再叫我写数据了，恢复成普通的监听读取状态
        loop_.modifyFd(client_fd, EPOLLIN | EPOLLET);
    }
}
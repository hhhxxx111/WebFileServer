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
            while (offset < file_size) {
                ssize_t sent = sendfile(active_fd, file_fd, &offset, file_size - offset);
                if (sent <= 0) break; // 这里我们稍后会专门解决 EAGAIN 的问题
            }
            close(file_fd);
        }
    } else {
        response = "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\n\r\n";
        write(active_fd, response.c_str(), response.length());
    }
}
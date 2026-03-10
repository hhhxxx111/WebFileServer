#include <iostream>
#include <cstring>      // for memset
#include <unistd.h>     // for close, sleep
#include <sys/socket.h> // for socket APIs
#include <netinet/in.h> // for sockaddr_in
#include <arpa/inet.h>  // for inet_ntoa

#define PORT 8080

int main() {
    // 1. 创建监听 Socket
    // AF_INET: IPv4
    // SOCK_STREAM: TCP 协议
    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd == -1) {
        std::cerr << "Failed to create socket!" << std::endl;
        return -1;
    }

    // 2. 设置端口复用 (非常重要！)
    // 在开发阶段我们经常重启服务器，如果不设置 SO_REUSEADDR，
    // 重启时系统会报错 "Address already in use"，导致端口被占用几分钟。
    int optval = 1;
    if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) == -1) {
        std::cerr << "Failed to set SO_REUSEADDR!" << std::endl;
        close(listen_fd);
        return -1;
    }

    // 3. 配置服务器的地址结构体
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr)); // 清零
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY); // 监听本机所有网卡 IP (0.0.0.0)
    server_addr.sin_port = htons(8080);              // 监听 8080 端口 (注意字节序转换)

    // 4. 将 Socket 与地址和端口绑定
    if (bind(listen_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        std::cerr << "Failed to bind port 8080!" << std::endl;
        perror("bind");
        close(listen_fd);
        return -1;
    }

    // 5. 开始监听
    // SOMAXCONN 是系统定义的最大半连接队列长度，通常是 128 或更大
    if (listen(listen_fd, SOMAXCONN) == -1) {
        std::cerr << "Failed to listen on socket!" << std::endl;
        perror("listen");
        close(listen_fd);
        return -1;
    }

    std::cout << "🚀 WebFileServer is successfully listening on 0.0.0.0:8080..." << std::endl;

    while (true) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);

        // 1. 阻塞等待客户端（比如浏览器）连接
        int client_fd = accept(listen_fd, (struct sockaddr*)&client_addr, &client_len);
        if (client_fd == -1) {
            std::cerr << "Failed to accept connection!" << std::endl;
            continue;
        }

        char client_ip[INET_ADDRSTRLEN]; // 准备一个足够长的数组
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
        std::cout << "收到连接: " << client_ip << std::endl;

        // 2. 读取浏览器发来的数据
        char buffer[4096] = {0}; // 准备一个 4K 的缓冲区
        ssize_t bytes_read = read(client_fd, buffer, sizeof(buffer) - 1);
        
        if (bytes_read > 0) {
            std::cout << "--- 浏览器发来的原始 HTTP 请求 ---" << std::endl;
            std::cout << buffer; // 打印原始报文
            std::cout << "-----------------------------------" << std::endl;

            // 3. 给浏览器随便回一句符合 HTTP 规范的响应，不然浏览器会一直转圈报错
            // 注意 HTTP 响应头的格式：状态行 + 响应头 + \r\n\r\n + 响应体
            std::string response = 
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: text/plain\r\n"
                "Content-Length: 12\r\n"
                "\r\n"
                "Hello World!";
                
            write(client_fd, response.c_str(), response.length());
        }

        // 4. 短连接：处理完马上关掉
        close(client_fd);
    }

    close(listen_fd);
    return 0;
}
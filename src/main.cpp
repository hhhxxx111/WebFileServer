#include "../include/HttpServer.h"

int main() {
    // 实例化一个运行在 8080 端口的服务器
    HttpServer server(8080);
    
    // 启动它！
    server.start();

    return 0;
}
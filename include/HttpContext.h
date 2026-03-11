#pragma once
#include <string>
#include <unordered_map>

// 1. 定义一个结构体，用来存放解析好的 HTTP 请求
struct HttpRequest {
    std::string method;  // 如 "GET"
    std::string path;    // 如 "/favicon.ico"
    std::string version; // 如 "HTTP/1.1"
    std::unordered_map<std::string, std::string> headers; // 存放请求头
    std::string body;    // 存放请求体（上传文件时会用到）
};

// 2. HTTP 状态机解析器
class HttpContext {
public:
    // 状态机的四个核心状态
    enum ParseState {
        STATE_REQUEST_LINE, // 正在解析第一行 (请求行)
        STATE_HEADERS,      // 正在解析请求头
        STATE_BODY,         // 正在解析请求体
        STATE_FINISH,       // 解析完成
        STATE_ERROR         // 解析出错
    };

    HttpContext();
    ~HttpContext() = default;

    // 喂给它从 epoll 读到的 raw_data，它返回是否解析完成
    bool parse(const std::string& raw_data);
    
    // 获取解析好的结构体
    const HttpRequest& getRequest() const { return request_; }
    ParseState getState() const { return state_; }
    
    // 复用 TCP 连接时（Keep-Alive），需要清空状态迎接下一个请求
    void clear(); 

private:
    ParseState state_;
    HttpRequest request_;
    std::string buffer_; // 内部缓冲区，处理 TCP 半包问题
    
    bool parseRequestLine(const std::string& line);
    bool parseHeader(const std::string& line);
};
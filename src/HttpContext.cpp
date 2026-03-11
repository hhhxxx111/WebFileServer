#include "../include/HttpContext.h"
#include <iostream>
#include <sstream>

HttpContext::HttpContext() : state_(STATE_REQUEST_LINE) {}

void HttpContext::clear() {
    state_ = STATE_REQUEST_LINE;
    request_ = HttpRequest();
    buffer_.clear();
}

// 核心解析逻辑：以 \r\n 为分隔符，逐行驱动状态机
bool HttpContext::parse(const std::string& raw_data) {
    buffer_ += raw_data;

    // 只要没完成且没出错，就一直尝试解析
    while (state_ != STATE_FINISH && state_ != STATE_ERROR) {
        // 寻找 HTTP 协议的标准换行符 \r\n
        size_t crlf_pos = buffer_.find("\r\n");
        if (crlf_pos == std::string::npos) {
            // 没找到换行符，说明数据不够一行，跳出循环，等 epoll 下次喂数据
            break;
        }

        // 提取出一整行
        std::string line = buffer_.substr(0, crlf_pos);
        // 从缓冲区中剔除刚才读出的那行和 \r\n 两个字符
        buffer_.erase(0, crlf_pos + 2); 

        // === 状态机流转 ===
        if (state_ == STATE_REQUEST_LINE) {
            if (parseRequestLine(line)) {
                state_ = STATE_HEADERS; // 成功解析第一行，进入下一阶段
            } else {
                state_ = STATE_ERROR;
            }
        } 
        else if (state_ == STATE_HEADERS) {
            if (line.empty()) {
                // 如果遇到空行（即连续的 \r\n\r\n），说明头部结束了！
                state_ = STATE_FINISH; // 目前还没处理 body，所以遇到空行就认为 GET 请求结束了
            } else {
                parseHeader(line); // 解析头部字段
            }
        }
    }
    
    return state_ == STATE_FINISH;
}

// 解析第一行，例如: "GET / HTTP/1.1"
bool HttpContext::parseRequestLine(const std::string& line) {
    // 使用 stringstream 按空格分割字符串
    std::istringstream iss(line);
    if (iss >> request_.method >> request_.path >> request_.version) {
        std::cout << "✅ 成功解析请求行 | Method: " << request_.method 
                  << " | Path: " << request_.path << std::endl;
        return true;
    }
    return false;
}

// 解析头部，例如: "Host: localhost:8080"
bool HttpContext::parseHeader(const std::string& line) {
    size_t colon_pos = line.find(':');
    if (colon_pos != std::string::npos) {
        std::string key = line.substr(0, colon_pos);
        // 跳过冒号后面的空格
        std::string value = line.substr(colon_pos + 2); 
        request_.headers[key] = value;
        return true;
    }
    return false;
}
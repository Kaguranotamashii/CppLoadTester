/*  文件说明:
 *  1. HttpProcessor 负责处理 HTTP 协议相关的功能
 *  2. 包括解析 HTTP 请求、构建 HTTP 响应等操作
 *  3. 处理 GET 和 POST 等不同类型的 HTTP 请求
 *  4. 继承自 EventBase 类,提供处理请求和发送响应的方法
 */
#ifndef HTTP_PROCESSOR_H
#define HTTP_PROCESSOR_H

#include <iostream>
#include <string>
#include <fstream>
#include <sys/socket.h>
#include <sys/sendfile.h>
#include <unistd.h>

#include "event_base.h"
#include "file_handle.h"
#include "../utils/utils.h"

// 前置声明
class FileHandle;

// HTTP 处理类,负责处理 HTTP 请求和构建 HTTP 响应
class HttpProcessor : public EventBase {
public:
    // 构造函数,传入客户端套接字、epoll 文件描述符和文件处理对象
    HttpProcessor(int clientFd, int epollFd, FileHandle* fileHandle);
    virtual ~HttpProcessor();

    // 处理 HTTP 请求的方法(用于接收请求)
    virtual void processRequest();
    
    // 发送 HTTP 响应的方法(用于发送响应)
    virtual void processResponse();

    // 用于构建状态行
    std::string buildStatusLine(const std::string &httpVersion, const std::string &statusCode, const std::string &statusDesc);
    
    // 构建消息头部
    std::string buildMessageHeader(const std::string &contentLength, const std::string &contentType, 
                                   const std::string &redirectLocation = "", const std::string &contentRange = "");

    // 重写基类的 process 方法
    virtual void process() override;

private:
    // 处理 GET 请求
    void handleGetRequest();
    
    // 处理 POST 请求
    void handlePostRequest();
    
    // 处理文件上传
    void handleFileUpload();
    
    // 构建文件列表响应
    void buildFileListResponse();
    
    // 构建文件下载响应
    void buildFileDownloadResponse(const std::string &fileName);
    
    // 构建文件删除后的响应
    void buildFileDeleteResponse(const std::string &fileName);
    
    // 构建重定向响应
    void buildRedirectResponse(const std::string &location = "/");

private:
    int m_clientFd;          // 客户端套接字
    int m_epollFd;           // epoll 文件描述符
    FileHandle* m_fileHandle; // 文件处理对象
    bool m_isRequest;        // 标记当前是处理请求还是发送响应
};

#endif // HTTP_PROCESSOR_H
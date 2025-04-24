/*  文件说明:
 *  1. ConnectionManager 负责管理网络连接相关的功能
 *  2. 包括接受新的连接、关闭连接、设置非阻塞等操作
 *  3. 继承自 EventBase 类,并提供处理连接请求的方法
 */
#ifndef CONNECTION_MANAGER_H
#define CONNECTION_MANAGER_H

#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <iostream>
#include <string>

#include "event_base.h"
#include "../utils/utils.h"

// 连接管理类,用于处理连接的接受、关闭和管理
class ConnectionManager : public EventBase {
public:
    // 构造函数,传入监听套接字和 epoll 文件描述符
    ConnectionManager(int listenFd, int epollFd);
    virtual ~ConnectionManager();

    // 重写基类的 process 方法,处理连接请求
    virtual void process() override;

    // 接受客户端连接的方法
    int acceptConnection();

    // 关闭客户端连接的方法
    void closeConnection(int clientFd);

    // 设置 epoll 监听事件
    void setEpollEvents(int clientFd, bool isRead, bool isWrite);

private:
    int m_listenFd;              // 保存监听套接字 
    int m_epollFd;               // epoll 文件描述符

    sockaddr_in clientAddr;      // 客户端地址
    socklen_t clientAddrLen;     // 客户端地址长度
};

#endif // CONNECTION_MANAGER_H
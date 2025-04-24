#include "connection_manager.h"

ConnectionManager::ConnectionManager(int listenFd, int epollFd) 
    : m_listenFd(listenFd), m_epollFd(epollFd) {
    // 构造函数实现,初始化成员变量
}

ConnectionManager::~ConnectionManager() {
    // 析构函数实现
}

void ConnectionManager::process() {
    // 处理连接请求的方法
    int clientFd = acceptConnection();
    if (clientFd < 0) {
        return;  // 接受连接失败,直接返回
    }

    // 将连接设置为非阻塞
    setNonBlocking(clientFd);

    // 将连接加入到监听,客户端套接字都设置为 EPOLLET 和 EPOLLONESHOT
    addWaitFd(m_epollFd, clientFd, true, true);
    std::cout << outHead("info") << "接受新连接 " << clientFd << " 成功" << std::endl;
}

int ConnectionManager::acceptConnection() {
    // 接受新的客户端连接
    clientAddrLen = sizeof(clientAddr);
    int clientFd = accept(m_listenFd, (sockaddr*)&clientAddr, &clientAddrLen);
    
    if(clientFd == -1) {
        std::cout << outHead("error") << "接受新连接失败" << std::endl;
        return -1;
    }
    
    return clientFd;
}

void ConnectionManager::closeConnection(int clientFd) {
    // 从 epoll 中删除对该连接的监听
    deleteWaitFd(m_epollFd, clientFd);
    
    // 关闭连接
    shutdown(clientFd, SHUT_RDWR);
    close(clientFd);
    
    std::cout << outHead("info") << "关闭客户端连接 " << clientFd << std::endl;
}

void ConnectionManager::setEpollEvents(int clientFd, bool isRead, bool isWrite) {
    // 设置 epoll 监听事件
    if (isRead && isWrite) {
        // 既监听读事件又监听写事件
        modifyWaitFd(m_epollFd, clientFd, true, true, true);
    } else if (isRead) {
        // 只监听读事件
        modifyWaitFd(m_epollFd, clientFd, true, true, false);
    } else if (isWrite) {
        // 只监听写事件
        modifyWaitFd(m_epollFd, clientFd, true, true, true);
    }
}
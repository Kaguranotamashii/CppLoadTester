/**
 * @file LoadTester.h
 * @brief 负载测试器类的声明
 */
#pragma once

#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>
#include <chrono>
#include <fstream>
#include <functional>

/**
 * @class LoadTester
 * @brief 负载测试工具核心类，用于执行HTTP请求测试
 */
class LoadTester {
public:
    /**
     * @brief 默认构造函数
     */
    LoadTester();

    /**
     * @brief 析构函数
     */
    ~LoadTester();

    /**
     * @brief 开始负载测试
     * @param testUrl 测试的URL
     * @param threads 线程数
     * @param requests 请求总数
     * @param logFilePath 日志文件路径
     * @return 如果成功开始测试返回true，否则返回false
     */
    bool start(const std::string& testUrl, int threads, int requests, const std::string& logFilePath);

    /**
     * @brief 停止当前运行的测试
     */
    void stop();

    /**
     * @brief 获取已完成的请求数
     * @return 已完成的请求数
     */
    int getCompletedRequests() const;

    /**
     * @brief 获取总请求数
     * @return 总请求数
     */
    int getTotalRequests() const;

    /**
     * @brief 获取成功的请求数
     * @return 成功的请求数
     */
    int getSuccessfulRequests() const;

    /**
     * @brief 获取成功率
     * @return 成功率百分比
     */
    double getSuccessRate() const;

    /**
     * @brief 检查测试是否正在运行
     * @return 如果测试正在运行返回true，否则返回false
     */
    bool isTestRunning() const;

    /**
     * @brief 设置状态更新回调函数
     * @param callback 回调函数，接收完成请求数、总请求数和成功率作为参数
     */
    void setStatusCallback(std::function<void(int, int, double)> callback);

    /**
     * @brief 获取最小响应时间
     * @return 最小响应时间（毫秒）
     */
    double getMinResponseTime() const;

    /**
     * @brief 获取最大响应时间
     * @return 最大响应时间（毫秒）
     */
    double getMaxResponseTime() const;

    /**
     * @brief 获取平均响应时间
     * @return 平均响应时间（毫秒）
     */
    double getAvgResponseTime() const;

private:
    /**
     * @brief 写入回调函数，用于接收HTTP响应
     */
    static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp);

    /**
     * @brief 记录日志
     * @param message 日志消息
     */
    void log(const std::string& message);

    /**
     * @brief 发送单个HTTP请求
     */
    void makeRequest();

    /**
     * @brief 工作线程函数
     */
    void workerThread();

    /**
     * @brief 计算响应时间统计信息
     */
    void calculateStatistics();

private:
    // 初始化顺序应与构造函数中的初始化顺序相匹配
    bool isRunning;                            ///< 测试是否正在运行
    std::atomic<int> completedRequests;        ///< 已完成的请求数
    std::atomic<int> successfulRequests;       ///< 成功的请求数
    double minResponseTime;                    ///< 最小响应时间
    double maxResponseTime;                    ///< 最大响应时间
    double avgResponseTime;                    ///< 平均响应时间

    std::string url;                           ///< 测试URL
    int numThreads;                            ///< 线程数
    int totalRequests;                         ///< 总请求数
    std::vector<std::thread> threads;          ///< 工作线程
    std::mutex logMutex;                       ///< 日志互斥锁
    std::ofstream logFile;                     ///< 日志文件流
    std::chrono::time_point<std::chrono::system_clock> startTime;  ///< 测试开始时间
    std::chrono::time_point<std::chrono::system_clock> endTime;    ///< 测试结束时间

    std::vector<double> responseTimes;         ///< 响应时间数组
    std::mutex responseTimesMutex;             ///< 响应时间互斥锁

    std::function<void(int, int, double)> statusCallback;  ///< 状态更新回调函数
};
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
#include <deque>

/**
 * @enum RequestStatus
 * @brief 请求状态枚举
 */
enum class RequestStatus {
    SUCCESS,    ///< 请求成功 (2xx状态码)
    FAILED,     ///< 请求失败 (非2xx状态码)
    REQ_ERROR   ///< 请求出错 (连接错误等)
};

/**
 * @struct RequestResult
 * @brief 单个请求的结果
 */
struct RequestResult {
    int id;                             ///< 请求ID
    RequestStatus status;               ///< 请求状态
    int statusCode;                     ///< HTTP状态码 (如果可用)
    std::string url;                    ///< 请求的URL
    double responseTime;                ///< 响应时间(毫秒)
    std::string errorMessage;           ///< 错误信息(如果有)
    std::chrono::system_clock::time_point timestamp; ///< 请求时间戳

    RequestResult(int _id, RequestStatus _status, int _code,
                 const std::string& _url, double _time,
                 const std::string& _error = "")
        : id(_id), status(_status), statusCode(_code), url(_url),
          responseTime(_time), errorMessage(_error),
          timestamp(std::chrono::system_clock::now()) {}
};

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
     * @brief 设置单个请求结果回调函数
     * @param callback 回调函数，接收RequestResult对象
     */
    void setRequestCallback(std::function<void(const RequestResult&)> callback);

    /**
     * @brief 获取最近的请求结果
     * @param count 要获取的结果数量
     * @return 请求结果的向量
     */
    std::vector<RequestResult> getRecentResults(int count = 10) const;

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

    /**
     * @brief 获取所有响应时间数据
     * @return 响应时间数组
     */
    std::vector<double> getResponseTimes() const;

    /**
     * @brief 打开并读取日志文件
     * @param logFilePath 日志文件路径
     * @return 日志内容
     */
    static std::string readLogFile(const std::string& logFilePath);

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
     * @return 请求结果
     */
    RequestResult makeRequest();

    /**
     * @brief 添加请求结果到历史记录
     * @param result 请求结果
     */
    void addResult(const RequestResult& result);

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
    std::atomic<int> requestIdCounter;         ///< 请求ID计数器
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
    mutable std::mutex responseTimesMutex;     ///< 响应时间互斥锁 (mutable以允许const方法使用)

    std::deque<RequestResult> requestHistory;  ///< 请求历史记录
    mutable std::mutex historyMutex;           ///< 历史记录互斥锁 (mutable以允许const方法使用)
    static const size_t MAX_HISTORY_SIZE = 100; ///< 最大历史记录数量

    std::function<void(int, int, double)> statusCallback;  ///< 状态更新回调函数
    std::function<void(const RequestResult&)> requestCallback; ///< 请求结果回调函数
};
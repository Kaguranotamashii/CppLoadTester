/**
 * @file LoadTester.cpp
 * @brief 负载测试器类的实现
 */
#include "../include/LoadTester.h"
#include <algorithm>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <fstream>
#include <curl/curl.h>

LoadTester::LoadTester()
    : isRunning(false),
      completedRequests(0),
      successfulRequests(0),
      requestIdCounter(0),
      minResponseTime(0),
      maxResponseTime(0),
      avgResponseTime(0) {
}

LoadTester::~LoadTester() {
    if (isRunning) {
        stop();
    }
}

bool LoadTester::start(const std::string& testUrl, int threadCount, int requests, const std::string& logFilePath) {
    if (isRunning) return false;

    url = testUrl;
    numThreads = threadCount;
    totalRequests = requests;
    completedRequests = 0;
    successfulRequests = 0;
    requestIdCounter = 0;
    isRunning = true;
    responseTimes.clear();

    {
        std::lock_guard<std::mutex> lock(historyMutex);
        requestHistory.clear();
    }

    // 初始化curl
    curl_global_init(CURL_GLOBAL_ALL);

    // 打开日志文件
    logFile.open(logFilePath, std::ios::out | std::ios::app);
    if (!logFile.is_open()) {
        std::cerr << "无法打开日志文件: " << logFilePath << std::endl;
        return false;
    }

    log("测试开始: URL=" + url + ", 线程数=" + std::to_string(numThreads) +
        ", 请求数=" + std::to_string(totalRequests));

    startTime = std::chrono::system_clock::now();

    // 确保线程向量是空的
    threads.clear();

    // 启动工作线程
    for (int i = 0; i < numThreads; i++) {
        threads.push_back(std::thread(&LoadTester::workerThread, this));
    }

    return true;
}

void LoadTester::stop() {
    isRunning = false;

    // 等待所有线程完成
    for (auto& t : threads) {
        if (t.joinable()) t.join();
    }

    threads.clear();

    endTime = std::chrono::system_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();

    // 记录摘要
    log("测试完成: " + std::to_string(completedRequests) + "/" + std::to_string(totalRequests) +
        " 请求已完成, " + std::to_string(successfulRequests) + " 成功 (" +
        std::to_string(successfulRequests * 100.0 / completedRequests) + "%)");
    log("测试持续时间: " + std::to_string(duration) + " 毫秒");

    // 计算统计信息
    calculateStatistics();

    // 记录响应时间统计
    log("响应时间: 最小=" + std::to_string(minResponseTime) + " 毫秒, 平均=" +
        std::to_string(avgResponseTime) + " 毫秒, 最大=" + std::to_string(maxResponseTime) + " 毫秒");

    logFile.close();
    curl_global_cleanup();
}

int LoadTester::getCompletedRequests() const {
    return completedRequests;
}

int LoadTester::getTotalRequests() const {
    return totalRequests;
}

int LoadTester::getSuccessfulRequests() const {
    return successfulRequests;
}

double LoadTester::getSuccessRate() const {
    if (completedRequests == 0) return 0.0;
    return (successfulRequests * 100.0) / completedRequests;
}

bool LoadTester::isTestRunning() const {
    return isRunning;
}

void LoadTester::setStatusCallback(std::function<void(int, int, double)> callback) {
    statusCallback = callback;
}

void LoadTester::setRequestCallback(std::function<void(const RequestResult&)> callback) {
    requestCallback = callback;
}

std::vector<RequestResult> LoadTester::getRecentResults(int count) const {
    std::lock_guard<std::mutex> lock(historyMutex);
    std::vector<RequestResult> results;

    // 取最近的count个结果
    int resultCount = std::min(count, static_cast<int>(requestHistory.size()));
    results.reserve(resultCount);

    // 从最新的开始复制
    auto it = requestHistory.begin();
    for (int i = 0; i < resultCount; ++i, ++it) {
        results.push_back(*it);
    }

    return results;
}

double LoadTester::getMinResponseTime() const {
    return minResponseTime;
}

double LoadTester::getMaxResponseTime() const {
    return maxResponseTime;
}

double LoadTester::getAvgResponseTime() const {
    return avgResponseTime;
}

std::vector<double> LoadTester::getResponseTimes() const {
    std::lock_guard<std::mutex> lock(responseTimesMutex);
    return responseTimes;
}

std::string LoadTester::readLogFile(const std::string& logFilePath) {
    std::ifstream file(logFilePath);
    if (!file.is_open()) {
        return "无法打开日志文件: " + logFilePath;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

size_t LoadTester::WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

void LoadTester::log(const std::string& message) {
    std::lock_guard<std::mutex> lock(logMutex);
    auto now = std::chrono::system_clock::now();
    auto now_c = std::chrono::system_clock::to_time_t(now);

    std::stringstream ss;
    ss << std::put_time(std::localtime(&now_c), "%Y-%m-%d %H:%M:%S") << " - " << message;

    logFile << ss.str() << std::endl;

    // 也可以发送到UI组件用于实时日志记录
    std::cout << ss.str() << std::endl;
}

void LoadTester::addResult(const RequestResult& result) {
    std::lock_guard<std::mutex> lock(historyMutex);
    // 添加到队列前面（最新的在前）
    requestHistory.push_front(result);

    // 限制历史记录大小
    if (requestHistory.size() > MAX_HISTORY_SIZE) {
        requestHistory.pop_back();
    }

    // 如果有回调，通知UI
    if (requestCallback) {
        requestCallback(result);
    }
}

RequestResult LoadTester::makeRequest() {
    CURL* curl;
    CURLcode res;
    std::string readBuffer;
    int requestId = ++requestIdCounter;

    curl = curl_easy_init();
    if (curl) {
        auto requestStart = std::chrono::high_resolution_clock::now();

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);  // 10秒超时

        res = curl_easy_perform(curl);

        auto requestEnd = std::chrono::high_resolution_clock::now();
        double elapsed = std::chrono::duration<double, std::milli>(requestEnd - requestStart).count();

        {
            std::lock_guard<std::mutex> lock(responseTimesMutex);
            responseTimes.push_back(elapsed);
        }

        completedRequests++;

        RequestResult result(requestId, RequestStatus::REQ_ERROR, 0, url, elapsed);

        if (res == CURLE_OK) {
            long response_code;
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
            result.statusCode = static_cast<int>(response_code);

            if (response_code >= 200 && response_code < 300) {
                successfulRequests++;
                result.status = RequestStatus::SUCCESS;
                log("请求成功: HTTP " + std::to_string(response_code) + " (" + std::to_string(elapsed) + " 毫秒)");
            } else {
                result.status = RequestStatus::FAILED;
                log("请求失败: HTTP " + std::to_string(response_code) + " (" + std::to_string(elapsed) + " 毫秒)");
            }
        } else {
            result.errorMessage = curl_easy_strerror(res);
            log("请求错误: " + result.errorMessage + " (" + std::to_string(elapsed) + " 毫秒)");
        }

        curl_easy_cleanup(curl);

        // 添加到历史记录
        addResult(result);

        return result;
    }

    // 如果curl初始化失败
    RequestResult errorResult(requestId, RequestStatus::REQ_ERROR, 0, url, 0, "CURL初始化失败");
    addResult(errorResult);
    return errorResult;
}

void LoadTester::workerThread() {
    while (isRunning && completedRequests < totalRequests) {
        RequestResult result = makeRequest();

        // 如果有状态回调，则调用它
        if (statusCallback) {
            statusCallback(completedRequests, totalRequests, getSuccessRate());
        }

        // 小延迟，防止目标服务器过载
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

void LoadTester::calculateStatistics() {
    if (responseTimes.empty()) {
        minResponseTime = 0;
        maxResponseTime = 0;
        avgResponseTime = 0;
        return;
    }

    minResponseTime = *std::min_element(responseTimes.begin(), responseTimes.end());
    maxResponseTime = *std::max_element(responseTimes.begin(), responseTimes.end());

    double sum = 0.0;
    for (const auto& time : responseTimes) {
        sum += time;
    }
    avgResponseTime = sum / responseTimes.size();
}
/**
* @file UIManager.h
 * @brief UI管理器类的头文件
 */
#ifndef UI_MANAGER_H
#define UI_MANAGER_H

#include <windows.h>
#include <string>
#include <fstream>
#include <functional>
#include <thread>
#include <vector>
#include <deque>
#include "LoadTester.h"

/**
 * @brief UI管理器类
 */
class UIManager {
public:
    // 构造函数和析构函数
    UIManager(HINSTANCE hInstance);
    ~UIManager();

    // 初始化UI
    bool initialize();

    // 运行消息循环
    int run();

private:
    // 常量
    static const int UPDATE_TIMER_ID = 1;
    static const int UPDATE_INTERVAL = 250;  // 毫秒
    static const int MAX_VISIBLE_REQUESTS = 10; // 可视化请求列表的最大数量

    // 窗口和控件句柄
    HINSTANCE hInstance;
    HWND hwndMain;
    HWND hwndUrlCombo;
    HWND hwndThreadsEdit;
    HWND hwndRequestsEdit;
    HWND hwndLogFileEdit;
    HWND hwndStartButton;
    HWND hwndStopButton;
    HWND hwndExitButton;
    HWND hwndProgressBar;
    HWND hwndStatusLabel;
    HWND hwndSuccessRateLabel;
    HWND hwndResponseTimeLabel;
    HWND hwndRequestListView;    // 新增：请求状态列表视图
    HWND hwndViewLogButton;      // 新增：查看日志按钮
    HWND hwndChartStatic;        // 新增：图表区域

    // 用于存储最近请求状态的数据结构
    std::deque<RequestResult> recentRequests;
    std::mutex requestsMutex;

    // 当前日志文件路径
    std::string currentLogFile;

    // 功能模块
    LoadTester tester;

    // UI状态
    bool updateTimerActive;

    // 窗口过程静态函数和静态实例指针
    static UIManager* instance;
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK LogDialogProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

    // UI创建函数
    bool registerWindowClass();
    bool createMainWindow();
    bool createControls();
    void initializeListView();
    void updateListView(const RequestResult& result);
    void updateListView();

    // 对话框函数
    void showLogDialog();

    // 绘图函数
    void drawChart(HDC hdc);

    // 事件处理函数
    LRESULT handleCommand(WPARAM wParam, LPARAM lParam);
    void handleStartTest();
    void handleStopTest();
    void handleViewLog();
    void handleExit();

    // UI更新函数
    void updateStatus(int completed, int total, double successRate);
    void showTestResults();
    void updateControlsState(bool testRunning);
    void handleRequestResult(const RequestResult& result);

    // 配置函数
    void saveCurrentConfig();
    void loadSavedConfig();
    void addUrlToComboBox(const std::string& url);
};

#endif // UI_MANAGER_H
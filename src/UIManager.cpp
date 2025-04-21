/**
 * @file UIManager.cpp
 * @brief UI管理器类的实现
 */
#include "../include/UIManager.h"
#include "../include/AppConfig.h"
#include <windowsx.h>
#include <sstream>
#include <iomanip>
#include <CommCtrl.h>
#include <shobjidl.h>
#include <shlwapi.h>
#include <ShlObj.h>  // 添加此头文件，包含SHGetFolderPathW函数

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "Shell32.lib")  // 添加此库，用于SHGetFolderPathW

// 控件ID
#define ID_URL_COMBO         101
#define ID_THREADS_EDIT      102
#define ID_REQUESTS_EDIT     103
#define ID_LOG_FILE_EDIT     104
#define ID_BROWSE_BUTTON     105
#define ID_START_BUTTON      106
#define ID_STOP_BUTTON       107
#define ID_EXIT_BUTTON       108
#define ID_PROGRESS_BAR      109
#define ID_STATUS_LABEL      110
#define ID_SUCCESS_RATE_LABEL 111
#define ID_RESPONSE_TIME_LABEL 112

// 窗口类名
#define WINDOW_CLASS_NAME    L"CppLoadTesterWindowClass"

// 静态实例指针，用于窗口过程回调
UIManager* UIManager::instance = nullptr;

UIManager::UIManager(HINSTANCE hInstance)
    : hInstance(hInstance),
      hwndMain(NULL),
      updateTimerActive(false) {
    // 保存实例指针
    instance = this;
}

UIManager::~UIManager() {
    if (updateTimerActive) {
        KillTimer(hwndMain, UPDATE_TIMER_ID);
        updateTimerActive = false;
    }

    // 停止正在运行的测试
    if (tester.isTestRunning()) {
        tester.stop();
    }

    // 保存配置
    saveCurrentConfig();

    // 清除实例指针
    instance = nullptr;
}

bool UIManager::initialize() {
    // 初始化通用控件
    INITCOMMONCONTROLSEX icc;
    icc.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icc.dwICC = ICC_BAR_CLASSES | ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&icc);

    // 注册窗口类
    if (!registerWindowClass()) {
        MessageBoxW(NULL, L"注册窗口类失败", L"错误", MB_ICONERROR);
        return false;
    }

    // 创建主窗口
    if (!createMainWindow()) {
        MessageBoxW(NULL, L"创建主窗口失败", L"错误", MB_ICONERROR);
        return false;
    }

    // 设置状态回调
    tester.setStatusCallback([this](int completed, int total, double successRate) {
        // 这个回调会在工作线程中调用，所以需要使用PostMessage来更新UI
        PostMessage(hwndMain, WM_USER, 0, 0);
    });

    // 加载配置
    loadSavedConfig();

    return true;
}

int UIManager::run() {
    ShowWindow(hwndMain, SW_SHOW);
    UpdateWindow(hwndMain);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return (int)msg.wParam;
}

void UIManager::updateStatus(int completed, int total, double successRate) {
    // 更新进度条
    double fraction = (double)completed / total;
    SendMessage(hwndProgressBar, PBM_SETPOS, (WPARAM)(int)(fraction * 100), 0);

    // 更新状态标签
    std::wstringstream wss;
    wss << L"运行中... " << completed << L"/" << total << L" 请求已完成";
    SetWindowTextW(hwndStatusLabel, wss.str().c_str());

    // 更新成功率标签
    wss.str(L"");
    wss << L"成功率: " << std::fixed << std::setprecision(2) << successRate << L"%";
    SetWindowTextW(hwndSuccessRateLabel, wss.str().c_str());
}

bool UIManager::createMainWindow() {
    // 创建主窗口
    hwndMain = CreateWindowExW(
        0,                              // 扩展风格
        WINDOW_CLASS_NAME,              // 窗口类名
        L"C++ 负载测试工具",            // 窗口标题
        WS_OVERLAPPEDWINDOW,            // 窗口风格
        CW_USEDEFAULT, CW_USEDEFAULT,   // 初始位置
        600, 400,                       // 初始大小
        NULL, NULL,                     // 父窗口和菜单句柄
        hInstance,                      // 应用程序实例
        NULL                            // 创建参数
    );

    if (!hwndMain) {
        return false;
    }

    // 创建控件
    if (!createControls()) {
        return false;
    }

    return true;
}

bool UIManager::createControls() {
    HFONT hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
    int y = 20;

    // URL组合框
    CreateWindowW(
        L"STATIC", L"URL:",
        WS_CHILD | WS_VISIBLE | SS_RIGHT,
        20, y, 80, 22,
        hwndMain, NULL, hInstance, NULL
    );

    hwndUrlCombo = CreateWindowW(
        WC_COMBOBOXW, L"",
        WS_CHILD | WS_VISIBLE | CBS_DROPDOWN | CBS_HASSTRINGS | WS_VSCROLL,
        110, y, 350, 200,
        hwndMain, (HMENU)ID_URL_COMBO, hInstance, NULL
    );

    // 线程数编辑框
    y += 30;
    CreateWindowW(
        L"STATIC", L"线程数:",
        WS_CHILD | WS_VISIBLE | SS_RIGHT,
        20, y, 80, 22,
        hwndMain, NULL, hInstance, NULL
    );

    hwndThreadsEdit = CreateWindowW(
        L"EDIT", L"10",
        WS_CHILD | WS_VISIBLE | WS_BORDER | ES_NUMBER,
        110, y, 100, 22,
        hwndMain, (HMENU)ID_THREADS_EDIT, hInstance, NULL
    );

    // 请求数编辑框
    y += 30;
    CreateWindowW(
        L"STATIC", L"请求数:",
        WS_CHILD | WS_VISIBLE | SS_RIGHT,
        20, y, 80, 22,
        hwndMain, NULL, hInstance, NULL
    );

    hwndRequestsEdit = CreateWindowW(
        L"EDIT", L"100",
        WS_CHILD | WS_VISIBLE | WS_BORDER | ES_NUMBER,
        110, y, 100, 22,
        hwndMain, (HMENU)ID_REQUESTS_EDIT, hInstance, NULL
    );

    // 日志文件编辑框
    y += 30;
    CreateWindowW(
        L"STATIC", L"日志文件:",
        WS_CHILD | WS_VISIBLE | SS_RIGHT,
        20, y, 80, 22,
        hwndMain, NULL, hInstance, NULL
    );

    hwndLogFileEdit = CreateWindowW(
        L"EDIT", L"loadtest.log",
        WS_CHILD | WS_VISIBLE | WS_BORDER,
        110, y, 300, 22,
        hwndMain, (HMENU)ID_LOG_FILE_EDIT, hInstance, NULL
    );

    // 浏览按钮
    CreateWindowW(
        L"BUTTON", L"浏览...",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        420, y, 60, 22,
        hwndMain, (HMENU)ID_BROWSE_BUTTON, hInstance, NULL
    );

    // 按钮
    y += 40;
    hwndStartButton = CreateWindowW(
        L"BUTTON", L"开始测试",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        110, y, 100, 30,
        hwndMain, (HMENU)ID_START_BUTTON, hInstance, NULL
    );

    hwndStopButton = CreateWindowW(
        L"BUTTON", L"停止测试",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_DISABLED,
        220, y, 100, 30,
        hwndMain, (HMENU)ID_STOP_BUTTON, hInstance, NULL
    );

    hwndExitButton = CreateWindowW(
        L"BUTTON", L"退出",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        330, y, 100, 30,
        hwndMain, (HMENU)ID_EXIT_BUTTON, hInstance, NULL
    );

    // 进度条
    y += 40;
    hwndProgressBar = CreateWindowW(
        PROGRESS_CLASSW, NULL,
        WS_CHILD | WS_VISIBLE | PBS_SMOOTH,
        110, y, 350, 20,
        hwndMain, (HMENU)ID_PROGRESS_BAR, hInstance, NULL
    );
    SendMessage(hwndProgressBar, PBM_SETRANGE, 0, MAKELPARAM(0, 100));

    // 状态标签
    y += 30;
    hwndStatusLabel = CreateWindowW(
        L"STATIC", L"就绪",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        110, y, 350, 22,
        hwndMain, (HMENU)ID_STATUS_LABEL, hInstance, NULL
    );

    // 成功率标签
    y += 30;
    hwndSuccessRateLabel = CreateWindowW(
        L"STATIC", L"成功率: 0.00%",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        110, y, 350, 22,
        hwndMain, (HMENU)ID_SUCCESS_RATE_LABEL, hInstance, NULL
    );

    // 响应时间标签
    y += 30;
    hwndResponseTimeLabel = CreateWindowW(
        L"STATIC", L"响应时间: 最小=0ms, 平均=0ms, 最大=0ms",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        110, y, 350, 22,
        hwndMain, (HMENU)ID_RESPONSE_TIME_LABEL, hInstance, NULL
    );

    // 设置字体
    for (HWND hwnd : {hwndUrlCombo, hwndThreadsEdit, hwndRequestsEdit, hwndLogFileEdit,
                      hwndStartButton, hwndStopButton, hwndExitButton,
                      hwndStatusLabel, hwndSuccessRateLabel, hwndResponseTimeLabel}) {
        SendMessage(hwnd, WM_SETFONT, (WPARAM)hFont, MAKELPARAM(TRUE, 0));
    }

    return true;
}

bool UIManager::registerWindowClass() {
    WNDCLASSEXW wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WindowProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = hInstance;
    wcex.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszMenuName = NULL;
    wcex.lpszClassName = WINDOW_CLASS_NAME;
    wcex.hIconSm = LoadIcon(NULL, IDI_APPLICATION);

    return RegisterClassExW(&wcex) != 0;
}

LRESULT CALLBACK UIManager::WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    // 使用静态实例指针
    if (instance) {
        if (uMsg == WM_COMMAND) {
            return instance->handleCommand(wParam, lParam);
        } else if (uMsg == WM_USER) {
            // 更新UI状态的消息
            if (instance->tester.isTestRunning()) {
                instance->updateStatus(
                    instance->tester.getCompletedRequests(),
                    instance->tester.getTotalRequests(),
                    instance->tester.getSuccessRate()
                );
            }
            return 0;
        } else if (uMsg == WM_TIMER && wParam == UPDATE_TIMER_ID) {
            if (instance->tester.isTestRunning()) {
                instance->updateStatus(
                    instance->tester.getCompletedRequests(),
                    instance->tester.getTotalRequests(),
                    instance->tester.getSuccessRate()
                );
            } else {
                // 测试完成
                KillTimer(hwnd, UPDATE_TIMER_ID);
                instance->updateTimerActive = false;
                instance->updateControlsState(false);
                instance->showTestResults();
            }
            return 0;
        }
    }

    switch (uMsg) {
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }

    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

LRESULT UIManager::handleCommand(WPARAM wParam, LPARAM lParam) {
    int id = LOWORD(wParam);
    int code = HIWORD(wParam);

    switch (id) {
        case ID_START_BUTTON:
            handleStartTest();
            return 0;

        case ID_STOP_BUTTON:
            handleStopTest();
            return 0;

        case ID_EXIT_BUTTON:
            handleExit();
            return 0;

        case ID_BROWSE_BUTTON:
            // 实现文件浏览对话框
            {
                IFileSaveDialog* pFileSave;

                // 创建保存对话框实例
                HRESULT hr = CoCreateInstance(CLSID_FileSaveDialog, NULL, CLSCTX_ALL,
                                              IID_PPV_ARGS(&pFileSave));

                if (SUCCEEDED(hr)) {
                    // 设置标题
                    pFileSave->SetTitle(L"选择日志文件位置");

                    // 设置默认扩展名
                    pFileSave->SetDefaultExtension(L"log");

                    // 设置过滤器
                    COMDLG_FILTERSPEC rgSpec[] = {
                        { L"日志文件 (*.log)", L"*.log" },
                        { L"文本文件 (*.txt)", L"*.txt" },
                        { L"所有文件 (*.*)", L"*.*" }
                    };
                    pFileSave->SetFileTypes(ARRAYSIZE(rgSpec), rgSpec);

                    // 显示对话框
                    hr = pFileSave->Show(hwndMain);

                    if (SUCCEEDED(hr)) {
                        // 获取结果
                        IShellItem* pItem;
                        hr = pFileSave->GetResult(&pItem);

                        if (SUCCEEDED(hr)) {
                            PWSTR pszFilePath;
                            hr = pItem->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath);

                            if (SUCCEEDED(hr)) {
                                // 设置文本框的内容
                                SetWindowTextW(hwndLogFileEdit, pszFilePath);
                                CoTaskMemFree(pszFilePath);
                            }

                            pItem->Release();
                        }
                    }

                    pFileSave->Release();
                }
            }
            return 0;
    }

    return 0;
}

void UIManager::handleStartTest() {
    // 获取UI中的设置
    wchar_t buffer[1024];

    // 获取URL
    GetWindowTextW(hwndUrlCombo, buffer, 1024);
    std::wstring wUrl(buffer);
    std::string url(wUrl.begin(), wUrl.end());

    // 获取线程数
    GetWindowTextW(hwndThreadsEdit, buffer, 1024);
    int threads = _wtoi(buffer);
    if (threads <= 0) threads = 1;

    // 获取请求数
    GetWindowTextW(hwndRequestsEdit, buffer, 1024);
    int requests = _wtoi(buffer);
    if (requests <= 0) requests = 1;

    // 获取日志文件路径
    GetWindowTextW(hwndLogFileEdit, buffer, 1024);
    std::wstring wLogFile(buffer);
    std::string logFile(wLogFile.begin(), wLogFile.end());

    // 保存配置
    AppConfig& config = AppConfig::getInstance();
    config.setString("DefaultURL", url);
    config.setInt("DefaultThreads", threads);
    config.setInt("DefaultRequests", requests);
    config.setString("DefaultLogFile", logFile);
    config.addRecentUrl(url);

    // 添加URL到下拉框
    addUrlToComboBox(url);

    // 开始测试
    if (tester.start(url, threads, requests, logFile)) {
        // 更新UI状态
        updateControlsState(true);

        // 启动定时器更新UI
        SetTimer(hwndMain, UPDATE_TIMER_ID, UPDATE_INTERVAL, NULL);
        updateTimerActive = true;
    } else {
        MessageBoxW(hwndMain, L"启动测试失败。请检查设置和网络连接。", L"错误", MB_ICONERROR);
    }
}

void UIManager::handleStopTest() {
    // 创建一个线程来停止测试，以避免UI冻结
    std::thread([this]() {
        tester.stop();

        // 发送消息通知主线程更新UI
        PostMessage(hwndMain, WM_USER + 1, 0, 0);
    }).detach();

    // 立即禁用停止按钮
    EnableWindow(hwndStopButton, FALSE);
}

void UIManager::handleExit() {
    // 如果有正在运行的测试，先停止
    if (tester.isTestRunning()) {
        if (MessageBoxW(hwndMain, L"测试正在运行，确定要退出吗？", L"确认", MB_YESNO | MB_ICONQUESTION) != IDYES) {
            return;
        }
        tester.stop();
    }

    // 保存配置
    saveCurrentConfig();

    // 销毁窗口
    DestroyWindow(hwndMain);
}

void UIManager::showTestResults() {
    // 更新状态标签
    SetWindowTextW(hwndStatusLabel, L"测试完成");

    // 更新成功率标签
    std::wstringstream wss;
    wss << L"成功率: " << std::fixed << std::setprecision(2) << tester.getSuccessRate() << L"%";
    SetWindowTextW(hwndSuccessRateLabel, wss.str().c_str());

    // 更新响应时间标签
    wss.str(L"");
    wss << L"响应时间: 最小=" << std::fixed << std::setprecision(2) << tester.getMinResponseTime()
        << L"ms, 平均=" << tester.getAvgResponseTime()
        << L"ms, 最大=" << tester.getMaxResponseTime() << L"ms";
    SetWindowTextW(hwndResponseTimeLabel, wss.str().c_str());

    // 进度条设置为100%
    SendMessage(hwndProgressBar, PBM_SETPOS, 100, 0);
}

void UIManager::updateControlsState(bool testRunning) {
    // 更新控件状态
    EnableWindow(hwndUrlCombo, !testRunning);
    EnableWindow(hwndThreadsEdit, !testRunning);
    EnableWindow(hwndRequestsEdit, !testRunning);
    EnableWindow(hwndLogFileEdit, !testRunning);
    EnableWindow(GetDlgItem(hwndMain, ID_BROWSE_BUTTON), !testRunning);
    EnableWindow(hwndStartButton, !testRunning);
    EnableWindow(hwndStopButton, testRunning);
}

void UIManager::saveCurrentConfig() {
    AppConfig& config = AppConfig::getInstance();

    // 用户的文档目录
    wchar_t documentsPath[MAX_PATH];
    // 使用CSIDL_MYDOCUMENTS替代CSIDL_PERSONAL
    SHGetFolderPathW(NULL, CSIDL_MYDOCUMENTS, NULL, 0, documentsPath);

    // 配置文件完整路径
    std::wstring configFilePath = std::wstring(documentsPath) + L"\\CppLoadTester.cfg";
    std::string configFile(configFilePath.begin(), configFilePath.end());

    // 保存配置
    config.saveToFile(configFile);
}

void UIManager::loadSavedConfig() {
    AppConfig& config = AppConfig::getInstance();

    // 用户的文档目录
    wchar_t documentsPath[MAX_PATH];
    // 使用CSIDL_MYDOCUMENTS替代CSIDL_PERSONAL
    SHGetFolderPathW(NULL, CSIDL_MYDOCUMENTS, NULL, 0, documentsPath);

    // 配置文件完整路径
    std::wstring configFilePath = std::wstring(documentsPath) + L"\\CppLoadTester.cfg";
    std::string configFile(configFilePath.begin(), configFilePath.end());

    // 加载配置
    if (config.loadFromFile(configFile)) {
        // 设置默认值
        std::wstring wUrl = std::wstring(config.getString("DefaultURL").begin(), config.getString("DefaultURL").end());
        SetWindowTextW(hwndUrlCombo, wUrl.c_str());

        SetWindowTextW(hwndThreadsEdit, std::to_wstring(config.getInt("DefaultThreads")).c_str());
        SetWindowTextW(hwndRequestsEdit, std::to_wstring(config.getInt("DefaultRequests")).c_str());

        std::wstring wLogFile = std::wstring(config.getString("DefaultLogFile").begin(), config.getString("DefaultLogFile").end());
        SetWindowTextW(hwndLogFileEdit, wLogFile.c_str());

        // 添加最近URLs到下拉框
        for (const auto& url : config.getRecentUrls()) {
            addUrlToComboBox(url);
        }
    }
}

void UIManager::addUrlToComboBox(const std::string& url) {
    // 转换为宽字符
    std::wstring wUrl(url.begin(), url.end());

    // 检查是否已经在列表中
    int count = ComboBox_GetCount(hwndUrlCombo);
    for (int i = 0; i < count; i++) {
        wchar_t buffer[1024];
        ComboBox_GetLBText(hwndUrlCombo, i, buffer);
        if (wUrl == buffer) {
            // 已经存在，则删除它（后面会重新添加到顶部）
            ComboBox_DeleteString(hwndUrlCombo, i);
            break;
        }
    }

    // 添加到顶部
    ComboBox_InsertString(hwndUrlCombo, 0, wUrl.c_str());
    ComboBox_SetCurSel(hwndUrlCombo, 0);

    // 限制列表大小
    while (ComboBox_GetCount(hwndUrlCombo) > 10) {
        ComboBox_DeleteString(hwndUrlCombo, 10);
    }
}
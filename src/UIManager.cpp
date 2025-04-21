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
#include <ShlObj.h>
#include <richedit.h>
#include <algorithm>

#include "StringConversion.h"

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "Shell32.lib")

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
#define ID_REQUEST_LIST      113
#define ID_VIEW_LOG_BUTTON   114
#define ID_CHART_STATIC      115

// 对话框IDs
#define IDD_LOG_DIALOG      1001
#define ID_LOG_EDIT         1002
#define ID_LOG_CLOSE        1003

// 列表视图列ID
#define COL_ID              0
#define COL_STATUS          1
#define COL_CODE            2
#define COL_TIME            3
#define COL_URL             4

// 窗口类名
#define WINDOW_CLASS_NAME    L"CppLoadTesterWindowClass"
#define LOG_DIALOG_CLASS     L"CppLoadTesterLogDialog"

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
    icc.dwICC = ICC_BAR_CLASSES | ICC_STANDARD_CLASSES | ICC_LISTVIEW_CLASSES;
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

    // 设置请求结果回调
    tester.setRequestCallback([this](const RequestResult& result) {
        // 发送请求结果到UI线程
        PostMessage(hwndMain, WM_USER + 1, reinterpret_cast<WPARAM>(new RequestResult(result)), 0);
    });

    // 加载配置
    loadSavedConfig();

    // 初始化列表视图
    initializeListView();

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

    // 更新响应时间标签
    wss.str(L"");
    wss << L"响应时间: 最小=" << std::fixed << std::setprecision(2) << tester.getMinResponseTime()
        << L"ms, 平均=" << tester.getAvgResponseTime()
        << L"ms, 最大=" << tester.getMaxResponseTime() << L"ms";
    SetWindowTextW(hwndResponseTimeLabel, wss.str().c_str());

    // 重绘图表区域
    InvalidateRect(hwndChartStatic, NULL, TRUE);
}

bool UIManager::createMainWindow() {
    // 创建主窗口
    hwndMain = CreateWindowExW(
        0,                              // 扩展风格
        WINDOW_CLASS_NAME,              // 窗口类名
        L"C++ 负载测试工具",            // 窗口标题
        WS_OVERLAPPEDWINDOW,            // 窗口风格
        CW_USEDEFAULT, CW_USEDEFAULT,   // 初始位置
        800, 650,                       // 初始大小(增大了)
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
        110, y, 450, 200,  // 增加宽度
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
        110, y, 350, 22,
        hwndMain, (HMENU)ID_LOG_FILE_EDIT, hInstance, NULL
    );

    // 浏览按钮
    CreateWindowW(
        L"BUTTON", L"浏览...",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        470, y, 60, 22,
        hwndMain, (HMENU)ID_BROWSE_BUTTON, hInstance, NULL
    );

    // 添加查看日志按钮
    hwndViewLogButton = CreateWindowW(
        L"BUTTON", L"查看日志",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        540, y, 80, 22,
        hwndMain, (HMENU)ID_VIEW_LOG_BUTTON, hInstance, NULL
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
        110, y, 450, 20,  // 增加宽度
        hwndMain, (HMENU)ID_PROGRESS_BAR, hInstance, NULL
    );
    SendMessage(hwndProgressBar, PBM_SETRANGE, 0, MAKELPARAM(0, 100));

    // 状态标签
    y += 30;
    hwndStatusLabel = CreateWindowW(
        L"STATIC", L"就绪",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        110, y, 450, 22,  // 增加宽度
        hwndMain, (HMENU)ID_STATUS_LABEL, hInstance, NULL
    );

    // 成功率标签
    y += 30;
    hwndSuccessRateLabel = CreateWindowW(
        L"STATIC", L"成功率: 0.00%",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        110, y, 450, 22,  // 增加宽度
        hwndMain, (HMENU)ID_SUCCESS_RATE_LABEL, hInstance, NULL
    );

    // 响应时间标签
    y += 30;
    hwndResponseTimeLabel = CreateWindowW(
        L"STATIC", L"响应时间: 最小=0ms, 平均=0ms, 最大=0ms",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        110, y, 450, 22,  // 增加宽度
        hwndMain, (HMENU)ID_RESPONSE_TIME_LABEL, hInstance, NULL
    );

    // 添加请求列表视图
    y += 40;
    CreateWindowW(
        L"STATIC", L"最近请求:",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        20, y, 450, 22,
        hwndMain, NULL, hInstance, NULL
    );

    y += 25;
    hwndRequestListView = CreateWindowW(
        WC_LISTVIEWW, L"",
        WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL | WS_BORDER | LVS_NOSORTHEADER,
        20, y, 740, 150,  // 宽列表
        hwndMain, (HMENU)ID_REQUEST_LIST, hInstance, NULL
    );

    // 添加响应时间图表区域
    y += 160;
    CreateWindowW(
        L"STATIC", L"响应时间图表:",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        20, y, 450, 22,
        hwndMain, NULL, hInstance, NULL
    );

    y += 25;
    hwndChartStatic = CreateWindowW(
        L"STATIC", L"",
        WS_CHILD | WS_VISIBLE | SS_OWNERDRAW | WS_BORDER,
        20, y, 740, 150,  // 图表区域
        hwndMain, (HMENU)ID_CHART_STATIC, hInstance, NULL
    );

    // 设置字体
    for (HWND hwnd : {hwndUrlCombo, hwndThreadsEdit, hwndRequestsEdit, hwndLogFileEdit,
                      hwndStartButton, hwndStopButton, hwndExitButton, hwndViewLogButton,
                      hwndStatusLabel, hwndSuccessRateLabel, hwndResponseTimeLabel}) {
        SendMessage(hwnd, WM_SETFONT, (WPARAM)hFont, MAKELPARAM(TRUE, 0));
    }

    return true;
}

void UIManager::initializeListView() {
    // 启用扩展样式
    ListView_SetExtendedListViewStyle(hwndRequestListView, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);

    // 添加列
    LVCOLUMNW lvc = {0};
    lvc.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM;
    lvc.fmt = LVCFMT_LEFT;

    // ID列
    lvc.iSubItem = COL_ID;
    lvc.pszText = const_cast<LPWSTR>(L"ID");
    lvc.cx = 50;
    ListView_InsertColumn(hwndRequestListView, COL_ID, &lvc);

    // 状态列
    lvc.iSubItem = COL_STATUS;
    lvc.pszText = const_cast<LPWSTR>(L"状态");
    lvc.cx = 80;
    ListView_InsertColumn(hwndRequestListView, COL_STATUS, &lvc);

    // 状态码列
    lvc.iSubItem = COL_CODE;
    lvc.pszText = const_cast<LPWSTR>(L"状态码");
    lvc.cx = 70;
    ListView_InsertColumn(hwndRequestListView, COL_CODE, &lvc);

    // 响应时间列
    lvc.iSubItem = COL_TIME;
    lvc.pszText = const_cast<LPWSTR>(L"响应时间(ms)");
    lvc.cx = 120;
    ListView_InsertColumn(hwndRequestListView, COL_TIME, &lvc);

    // URL列
    lvc.iSubItem = COL_URL;
    lvc.pszText = const_cast<LPWSTR>(L"URL");
    lvc.cx = 400;
    ListView_InsertColumn(hwndRequestListView, COL_URL, &lvc);
}

void UIManager::updateListView(const RequestResult& result) {
    // 将结果添加到最近请求列表
    {
        std::lock_guard<std::mutex> lock(requestsMutex);
        recentRequests.push_front(result);

        // 限制列表大小
        if (recentRequests.size() > MAX_VISIBLE_REQUESTS) {
            recentRequests.pop_back();
        }
    }

    // 更新列表视图
    updateListView();
}

void UIManager::updateListView() {
    // 清空列表视图
    ListView_DeleteAllItems(hwndRequestListView);

    std::lock_guard<std::mutex> lock(requestsMutex);

    // 添加项目到列表视图
    for (size_t i = 0; i < recentRequests.size(); ++i) {
        const auto& req = recentRequests[i];

        // 创建列表项
        LVITEMW lvi = {0};
        lvi.mask = LVIF_TEXT | LVIF_PARAM;
        lvi.iItem = static_cast<int>(i);
        lvi.iSubItem = 0;

        // ID列
        wchar_t idStr[16];
        swprintf_s(idStr, L"%d", req.id);
        lvi.pszText = idStr;
        lvi.lParam = static_cast<LPARAM>(req.id);
        int itemIndex = ListView_InsertItem(hwndRequestListView, &lvi);

        // 状态列
        const wchar_t* statusText;
        switch (req.status) {
            case RequestStatus::SUCCESS:
                statusText = L"成功";
                ListView_SetItemText(hwndRequestListView, itemIndex, COL_STATUS, const_cast<LPWSTR>(statusText));
                // 设置行颜色为绿色 (通过设置背景色)
                {
                    LVITEMW lvItem = {0};
                    lvItem.mask = LVIF_PARAM;
                    lvItem.iItem = itemIndex;
                    lvItem.lParam = RGB(200, 255, 200);  // 浅绿色
                    ListView_SetItem(hwndRequestListView, &lvItem);
                }
                break;
            case RequestStatus::FAILED:
                statusText = L"失败";
                ListView_SetItemText(hwndRequestListView, itemIndex, COL_STATUS, const_cast<LPWSTR>(statusText));
                // 设置行颜色为橙色
                {
                    LVITEMW lvItem = {0};
                    lvItem.mask = LVIF_PARAM;
                    lvItem.iItem = itemIndex;
                    lvItem.lParam = RGB(255, 230, 180);  // 浅橙色
                    ListView_SetItem(hwndRequestListView, &lvItem);
                }
                break;
            case RequestStatus::REQ_ERROR:
                statusText = L"错误";
                ListView_SetItemText(hwndRequestListView, itemIndex, COL_STATUS, const_cast<LPWSTR>(statusText));
                // 设置行颜色为红色
                {
                    LVITEMW lvItem = {0};
                    lvItem.mask = LVIF_PARAM;
                    lvItem.iItem = itemIndex;
                    lvItem.lParam = RGB(255, 200, 200);  // 浅红色
                    ListView_SetItem(hwndRequestListView, &lvItem);
                }
                break;
        }

        // 状态码列
        if (req.statusCode > 0) {
            wchar_t codeStr[16];
            swprintf_s(codeStr, L"%d", req.statusCode);
            ListView_SetItemText(hwndRequestListView, itemIndex, COL_CODE, codeStr);
        } else {
            ListView_SetItemText(hwndRequestListView, itemIndex, COL_CODE, const_cast<LPWSTR>(L"-"));
        }

        // 响应时间列
        wchar_t timeStr[32];
        swprintf_s(timeStr, L"%.2f", req.responseTime);
        ListView_SetItemText(hwndRequestListView, itemIndex, COL_TIME, timeStr);

        // URL列 - 使用安全转换
        std::wstring wUrl = stringToWstring(req.url);
        ListView_SetItemText(hwndRequestListView, itemIndex, COL_URL, const_cast<LPWSTR>(wUrl.c_str()));
    }
}

void UIManager::drawChart(HDC hdc) {
    // 获取图表区域大小
    RECT rect;
    GetClientRect(hwndChartStatic, &rect);

    int width = rect.right - rect.left;
    int height = rect.bottom - rect.top;

    // 创建白色背景
    HBRUSH hBrush = CreateSolidBrush(RGB(255, 255, 255));
    FillRect(hdc, &rect, hBrush);
    DeleteObject(hBrush);

    // 获取响应时间数据
    std::vector<double> times = tester.getResponseTimes();

    if (times.empty()) {
        // 无数据时显示提示
        SetTextColor(hdc, RGB(100, 100, 100));
        SetBkMode(hdc, TRANSPARENT);
        TextOutW(hdc, width / 2 - 80, height / 2 - 10, L"无响应时间数据", 14);
        return;
    }

    // 找出最大和最小值
    double maxTime = *std::max_element(times.begin(), times.end());
    double minTime = *std::min_element(times.begin(), times.end());

    // 为坐标轴留出空间
    int leftMargin = 50;
    int bottomMargin = 30;
    int rightMargin = 20;
    int topMargin = 20;

    int chartWidth = width - leftMargin - rightMargin;
    int chartHeight = height - topMargin - bottomMargin;

    // 绘制坐标轴
    HPEN axisPen = CreatePen(PS_SOLID, 1, RGB(0, 0, 0));
    HPEN oldPen = (HPEN)SelectObject(hdc, axisPen);

    // Y轴
    MoveToEx(hdc, leftMargin, topMargin, NULL);
    LineTo(hdc, leftMargin, height - bottomMargin);

    // X轴
    MoveToEx(hdc, leftMargin, height - bottomMargin, NULL);
    LineTo(hdc, width - rightMargin, height - bottomMargin);

    // 绘制刻度和标签
    SetTextColor(hdc, RGB(0, 0, 0));
    SetBkMode(hdc, TRANSPARENT);

    // Y轴刻度
    for (int i = 0; i <= 5; i++) {
        int y = height - bottomMargin - (i * chartHeight / 5);
        double value = minTime + (i * (maxTime - minTime) / 5);

        MoveToEx(hdc, leftMargin - 5, y, NULL);
        LineTo(hdc, leftMargin, y);

        wchar_t label[16];
        swprintf_s(label, L"%.1f", value);
        TextOutW(hdc, leftMargin - 45, y - 10, label, (int)wcslen(label));
    }

    // X轴刻度 - 显示请求编号
    int maxDisplayedPoints = std::min(50, (int)times.size());
    int step = std::max(1, (int)times.size() / maxDisplayedPoints);

    for (int i = 0; i < maxDisplayedPoints; i++) {
        int index = i * step;
        if (index >= (int)times.size()) break;

        int x = leftMargin + (i * chartWidth / maxDisplayedPoints);

        MoveToEx(hdc, x, height - bottomMargin, NULL);
        LineTo(hdc, x, height - bottomMargin + 5);

        if (i % 5 == 0) {  // 只在每5个点绘制标签以避免拥挤
            wchar_t label[16];
            swprintf_s(label, L"%d", index + 1);
            TextOutW(hdc, x - 10, height - bottomMargin + 10, label, (int)wcslen(label));
        }
    }

    // 添加轴标签
    TextOutW(hdc, width / 2 - 30, height - 15, L"请求序号", 10);

    // 旋转文本以绘制Y轴标签
    LOGFONT logFont = {0};
    logFont.lfHeight = 14;
    logFont.lfEscapement = 900;  // 90度旋转
    wcscpy_s(logFont.lfFaceName, L"宋体");
    HFONT font = CreateFontIndirect(&logFont);
    HFONT oldFont = (HFONT)SelectObject(hdc, font);

    TextOutW(hdc, 15, height / 2 - 60, L"响应时间 (毫秒)", 16);

    SelectObject(hdc, oldFont);
    DeleteObject(font);

    // 绘制数据点和连线
    HPEN dataPen = CreatePen(PS_SOLID, 2, RGB(0, 120, 215));  // 蓝色
    SelectObject(hdc, dataPen);

    HBRUSH pointBrush = CreateSolidBrush(RGB(0, 120, 215));

    bool first = true;
    int prevX = 0, prevY = 0;

    for (int i = 0; i < maxDisplayedPoints; i++) {
        int index = i * step;
        if (index >= (int)times.size()) break;

        double value = times[index];
        int x = leftMargin + (i * chartWidth / maxDisplayedPoints);
        int y = height - bottomMargin - (int)((value - minTime) * chartHeight / (maxTime - minTime));

        // 绘制点
        Ellipse(hdc, x - 3, y - 3, x + 3, y + 3);

        // 连线
        if (!first) {
            MoveToEx(hdc, prevX, prevY, NULL);
            LineTo(hdc, x, y);
        }

        first = false;
        prevX = x;
        prevY = y;
    }

    // 清理
    SelectObject(hdc, oldPen);
    DeleteObject(dataPen);
    DeleteObject(axisPen);
    DeleteObject(pointBrush);
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
        } else if (uMsg == WM_USER + 1) {
            // 请求结果消息
            RequestResult* result = reinterpret_cast<RequestResult*>(wParam);
            if (result) {
                instance->handleRequestResult(*result);
                delete result;  // 删除创建的请求结果对象
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
        } else if (uMsg == WM_DRAWITEM) {
            // 处理自定义绘制
            DRAWITEMSTRUCT* pDIS = (DRAWITEMSTRUCT*)lParam;
            if (pDIS->hwndItem == instance->hwndChartStatic) {
                instance->drawChart(pDIS->hDC);
                return TRUE;
            }
        }
    }

    switch (uMsg) {
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }

    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

LRESULT CALLBACK UIManager::LogDialogProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_INITDIALOG:
            return TRUE;

        case WM_COMMAND:
            if (LOWORD(wParam) == ID_LOG_CLOSE || LOWORD(wParam) == IDCANCEL) {
                EndDialog(hwnd, 0);
                return TRUE;
            }
            break;

        case WM_CLOSE:
            EndDialog(hwnd, 0);
            return TRUE;
    }

    return FALSE;
}

void UIManager::handleRequestResult(const RequestResult& result) {
    // 更新请求列表视图
    updateListView(result);

    // 如果这是第一个请求，立即更新状态
    if (result.id == 1) {
        updateStatus(
            tester.getCompletedRequests(),
            tester.getTotalRequests(),
            tester.getSuccessRate()
        );
    }
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

        case ID_VIEW_LOG_BUTTON:
            handleViewLog();
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
    std::string url = wstringToString(wUrl);  // 使用安全转换

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
    std::string logFile = wstringToString(wLogFile);  // 使用安全转换
    currentLogFile = logFile;  // 保存当前日志文件路径

    // 清空列表视图
    {
        std::lock_guard<std::mutex> lock(requestsMutex);
        recentRequests.clear();
    }
    ListView_DeleteAllItems(hwndRequestListView);

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
        PostMessage(hwndMain, WM_USER + 2, 0, 0);
    }).detach();

    // 立即禁用停止按钮
    EnableWindow(hwndStopButton, FALSE);
}

void UIManager::handleViewLog() {
    // 检查日志文件是否存在
    wchar_t buffer[1024];
    GetWindowTextW(hwndLogFileEdit, buffer, 1024);
    std::wstring wLogFile(buffer);
    std::string logFile(wLogFile.begin(), wLogFile.end());

    std::ifstream file(logFile);
    if (!file.is_open()) {
        MessageBoxW(hwndMain, L"无法打开日志文件，请确认文件存在且可访问。", L"错误", MB_ICONERROR);
        return;
    }
    file.close();

    // 读取日志内容
    std::string logContent = LoadTester::readLogFile(logFile);

    // 创建并显示日志对话框
    showLogDialog();
}

void UIManager::showLogDialog() {
    // 创建对话框模板
    DLGTEMPLATE dlgTemplate;
    ZeroMemory(&dlgTemplate, sizeof(DLGTEMPLATE));
    dlgTemplate.style = WS_POPUP | WS_CAPTION | WS_SYSMENU | DS_MODALFRAME;
    dlgTemplate.cx = 500;
    dlgTemplate.cy = 350;
    dlgTemplate.cdit = 2;  // 两个控件：编辑框和按钮

    // 创建对话框
    HWND hLogDialog = CreateDialogIndirectW(
        hInstance,
        &dlgTemplate,
        hwndMain,
        LogDialogProc
    );

    if (!hLogDialog) {
        MessageBoxW(hwndMain, L"创建日志对话框失败。", L"错误", MB_ICONERROR);
        return;
    }

    // 设置对话框标题
    SetWindowTextW(hLogDialog, L"查看日志");

    // 获取对话框客户区大小
    RECT rect;
    GetClientRect(hLogDialog, &rect);
    int width = rect.right - rect.left;
    int height = rect.bottom - rect.top;

    // 创建富文本编辑框控件
    HWND hLogEdit = CreateWindowExW(
        0, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL | ES_AUTOHSCROLL,
        10, 10, width - 20, height - 50,
        hLogDialog, (HMENU)ID_LOG_EDIT, hInstance, NULL
    );

    // 创建关闭按钮
    HWND hCloseButton = CreateWindowW(
        L"BUTTON", L"关闭",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        width / 2 - 40, height - 35, 80, 25,
        hLogDialog, (HMENU)ID_LOG_CLOSE, hInstance, NULL
    );

    // 设置字体
    HFONT hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
    SendMessage(hLogEdit, WM_SETFONT, (WPARAM)hFont, MAKELPARAM(TRUE, 0));
    SendMessage(hCloseButton, WM_SETFONT, (WPARAM)hFont, MAKELPARAM(TRUE, 0));

    // 读取日志内容
    std::string logContent = LoadTester::readLogFile(currentLogFile);

    // 将内容转换为宽字符 - 使用安全转换
    std::wstring wLogContent = stringToWstring(logContent);

    // 设置文本
    SetWindowTextW(hLogEdit, wLogContent.c_str());

    // 显示对话框
    ShowWindow(hLogDialog, SW_SHOW);

    // 模态对话框消息循环
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        if (!IsDialogMessage(hLogDialog, &msg)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }
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

    // 更新图表
    InvalidateRect(hwndChartStatic, NULL, TRUE);

    // 完成测试后展示总体结果对话框
    std::wstringstream resultMsg;
    resultMsg << L"测试完成!\n\n";
    resultMsg << L"总请求数: " << tester.getTotalRequests() << L"\n";
    resultMsg << L"完成请求数: " << tester.getCompletedRequests() << L"\n";
    resultMsg << L"成功请求数: " << tester.getSuccessfulRequests() << L"\n";
    resultMsg << L"成功率: " << std::fixed << std::setprecision(2) << tester.getSuccessRate() << L"%\n\n";
    resultMsg << L"响应时间统计:\n";
    resultMsg << L"  最小: " << std::fixed << std::setprecision(2) << tester.getMinResponseTime() << L" ms\n";
    resultMsg << L"  最大: " << std::fixed << std::setprecision(2) << tester.getMaxResponseTime() << L" ms\n";
    resultMsg << L"  平均: " << std::fixed << std::setprecision(2) << tester.getAvgResponseTime() << L" ms\n\n";
    // 修复引号问题
    resultMsg << L"您可以通过点击\"查看日志\"按钮查看详细日志。";

    MessageBoxW(hwndMain, resultMsg.str().c_str(), L"测试结果", MB_ICONINFORMATION);
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
    EnableWindow(hwndViewLogButton, !testRunning);
}

// 替换saveCurrentConfig函数中的字符串转换
void UIManager::saveCurrentConfig() {
    AppConfig& config = AppConfig::getInstance();

    // 用户的文档目录
    wchar_t documentsPath[MAX_PATH];
    // 使用CSIDL_MYDOCUMENTS替代CSIDL_PERSONAL
    SHGetFolderPathW(NULL, CSIDL_MYDOCUMENTS, NULL, 0, documentsPath);

    // 配置文件完整路径
    std::wstring configFilePath = std::wstring(documentsPath) + L"\\CppLoadTester.cfg";
    std::string configFile = wstringToString(configFilePath);  // 使用安全转换

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
    std::string configFile = wstringToString(configFilePath);  // 使用安全转换

    // 加载配置
    if (config.loadFromFile(configFile)) {
        // 设置默认值 - 使用安全转换
        std::wstring wUrl = stringToWstring(config.getString("DefaultURL"));
        SetWindowTextW(hwndUrlCombo, wUrl.c_str());

        SetWindowTextW(hwndThreadsEdit, std::to_wstring(config.getInt("DefaultThreads")).c_str());
        SetWindowTextW(hwndRequestsEdit, std::to_wstring(config.getInt("DefaultRequests")).c_str());

        std::wstring wLogFile = stringToWstring(config.getString("DefaultLogFile"));
        SetWindowTextW(hwndLogFileEdit, wLogFile.c_str());
        currentLogFile = config.getString("DefaultLogFile");

        // 添加最近URLs到下拉框
        for (const auto& url : config.getRecentUrls()) {
            addUrlToComboBox(url);
        }
    }
}


void UIManager::addUrlToComboBox(const std::string& url) {
    // 转换为宽字符 - 使用安全转换
    std::wstring wUrl = stringToWstring(url);

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

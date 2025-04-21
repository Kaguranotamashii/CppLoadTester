/**
* @file main.cpp
 * @brief 主入口函数
 */
#include <windows.h>
#include <iostream>
#include <objbase.h>
#include "../include/UIManager.h"

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // 初始化COM
    HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    if (FAILED(hr)) {
        MessageBoxW(NULL, L"COM初始化失败", L"错误", MB_ICONERROR);
        return 1;
    }

    // 创建并初始化UI管理器
    UIManager uiManager(hInstance);
    if (!uiManager.initialize()) {
        MessageBoxW(NULL, L"UI初始化失败", L"错误", MB_ICONERROR);
        CoUninitialize();
        return 1;
    }

    // 运行消息循环
    int result = uiManager.run();

    // 清理COM
    CoUninitialize();

    return result;
}
/**
* @file main.cpp
 * @brief 主入口函数
 */
#include <windows.h>
#include <iostream>
#include <objbase.h>
#include <fstream>
#include "../include/UIManager.h"

// 写入错误日志
void writeErrorLog(const std::string& error) {
    std::ofstream logFile("error.log", std::ios::app);
    if (logFile.is_open()) {
        SYSTEMTIME st;
        GetLocalTime(&st);

        logFile << st.wYear << "-" << st.wMonth << "-" << st.wDay << " "
                << st.wHour << ":" << st.wMinute << ":" << st.wSecond
                << " - ERROR: " << error << std::endl;

        logFile.close();
    }
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // 创建启动日志
    std::ofstream startupLog("startup.log");
    startupLog << "程序启动..." << std::endl;

    try {
        // 初始化COM
        startupLog << "正在初始化COM..." << std::endl;
        HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
        if (FAILED(hr)) {
            startupLog << "COM初始化失败，错误码: " << hr << std::endl;
            MessageBoxW(NULL, L"COM初始化失败", L"错误", MB_ICONERROR);
            return 1;
        }
        startupLog << "COM初始化成功" << std::endl;

        // 创建并初始化UI管理器
        startupLog << "创建UI管理器..." << std::endl;
        UIManager uiManager(hInstance);

        startupLog << "初始化UI管理器..." << std::endl;
        if (!uiManager.initialize()) {
            startupLog << "UI初始化失败" << std::endl;
            MessageBoxW(NULL, L"UI初始化失败", L"错误", MB_ICONERROR);
            CoUninitialize();
            return 1;
        }
        startupLog << "UI初始化成功" << std::endl;

        // 运行消息循环
        startupLog << "开始消息循环..." << std::endl;
        int result = uiManager.run();
        startupLog << "消息循环结束，退出码: " << result << std::endl;

        // 清理COM
        CoUninitialize();
        startupLog << "程序正常退出" << std::endl;
        return result;
    }
    catch (const std::length_error& e) {
        startupLog << "捕获到std::length_error异常: " << e.what() << std::endl;
        writeErrorLog(std::string("std::length_error: ") + e.what());
        MessageBoxA(NULL, ("程序发生字符串长度错误: " + std::string(e.what())).c_str(), "错误", MB_ICONERROR);
    }
    catch (const std::exception& e) {
        startupLog << "捕获到std::exception异常: " << e.what() << std::endl;
        writeErrorLog(std::string("std::exception: ") + e.what());
        MessageBoxA(NULL, ("程序发生错误: " + std::string(e.what())).c_str(), "错误", MB_ICONERROR);
    }
    catch (...) {
        startupLog << "捕获到未知异常" << std::endl;
        writeErrorLog("Unknown exception");
        MessageBoxW(NULL, L"程序发生未知错误", L"错误", MB_ICONERROR);
    }

    // 清理COM
    CoUninitialize();
    return 1;
}
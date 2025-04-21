/**
 * @file StringConversion.h
 * @brief 安全的字符串转换函数
 */
#pragma once

#include <string>
#include <windows.h>
#include <vector>

/**
 * 将 UTF-16 (wstring) 转换为 UTF-8 (string)
 *
 * @param wstr 输入的 UTF-16 字符串
 * @return 转换后的 UTF-8 字符串
 */
inline std::string wstringToString(const std::wstring& wstr) {
    if (wstr.empty()) {
        return std::string();
    }

    // 获取所需缓冲区大小
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, wstr.data(), (int)wstr.size(),
                                          nullptr, 0, nullptr, nullptr);
    if (size_needed <= 0) {
        return std::string(); // 转换失败
    }

    // 分配缓冲区
    std::string result(size_needed, 0);

    // 执行转换
    WideCharToMultiByte(CP_UTF8, 0, wstr.data(), (int)wstr.size(),
                        &result[0], size_needed, nullptr, nullptr);

    return result;
}

/**
 * 将 UTF-8 (string) 转换为 UTF-16 (wstring)
 *
 * @param str 输入的 UTF-8 字符串
 * @return 转换后的 UTF-16 字符串
 */
inline std::wstring stringToWstring(const std::string& str) {
    if (str.empty()) {
        return std::wstring();
    }

    // 获取所需缓冲区大小
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, str.data(), (int)str.size(),
                                          nullptr, 0);
    if (size_needed <= 0) {
        return std::wstring(); // 转换失败
    }

    // 分配缓冲区
    std::wstring result(size_needed, 0);

    // 执行转换
    MultiByteToWideChar(CP_UTF8, 0, str.data(), (int)str.size(),
                        &result[0], size_needed);

    return result;
}

/**
 * 安全地从 Windows 控件获取文本
 *
 * @param hwnd 控件句柄
 * @return 控件文本内容
 */
inline std::string getWindowTextAsString(HWND hwnd) {
    if (!hwnd) {
        return std::string();
    }

    // 获取文本长度
    int length = GetWindowTextLengthW(hwnd);
    if (length <= 0) {
        return std::string();
    }

    // 获取文本
    std::wstring buffer(length + 1, 0);
    GetWindowTextW(hwnd, &buffer[0], length + 1);
    buffer.resize(length); // 移除末尾多余的 null 字符

    // 转换为 UTF-8
    return wstringToString(buffer);
}
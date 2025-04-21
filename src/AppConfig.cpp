/**
 * @file AppConfig.cpp
 * @brief 应用配置类的实现
 */
#include "../include/AppConfig.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <vector>

AppConfig& AppConfig::getInstance() {
    static AppConfig instance;
    return instance;
}

AppConfig::AppConfig() {
    // 设置默认值
    setString("DefaultURL", "http://example.com");
    setInt("DefaultThreads", 10);
    setInt("DefaultRequests", 100);
    setString("DefaultLogFile", "loadtest.log");
}

bool AppConfig::loadFromFile(const std::string& filePath) {
    std::ifstream file(filePath);
    if (!file.is_open()) {
        return false;
    }

    configMap.clear();
    recentUrls.clear();

    std::string line;
    bool recentUrlsSection = false;

    while (std::getline(file, line)) {
        // 跳过注释和空行
        if (line.empty() || line[0] == '#') {
            continue;
        }

        if (line == "[RecentURLs]") {
            recentUrlsSection = true;
            continue;
        } else if (line[0] == '[') {
            recentUrlsSection = false;
            continue;
        }

        if (recentUrlsSection) {
            recentUrls.push_back(line);
            if (recentUrls.size() >= MAX_RECENT_URLS) {
                break;
            }
        } else {
            size_t pos = line.find('=');
            if (pos != std::string::npos) {
                std::string key = line.substr(0, pos);
                std::string value = line.substr(pos + 1);
                configMap[key] = value;
            }
        }
    }

    file.close();
    return true;
}

bool AppConfig::saveToFile(const std::string& filePath) {
    std::ofstream file(filePath);
    if (!file.is_open()) {
        return false;
    }

    file << "# 应用程序配置文件\n";
    file << "# 自动生成于 " << __DATE__ << " " << __TIME__ << "\n\n";

    file << "[General]\n";
    for (const auto& pair : configMap) {
        file << pair.first << "=" << pair.second << "\n";
    }

    file << "\n[RecentURLs]\n";
    for (const auto& url : recentUrls) {
        file << url << "\n";
    }

    file.close();
    return true;
}

std::string AppConfig::getString(const std::string& key, const std::string& defaultValue) const {
    auto it = configMap.find(key);
    if (it != configMap.end()) {
        return it->second;
    }
    return defaultValue;
}

int AppConfig::getInt(const std::string& key, int defaultValue) const {
    auto it = configMap.find(key);
    if (it != configMap.end()) {
        try {
            return std::stoi(it->second);
        } catch (...) {
            return defaultValue;
        }
    }
    return defaultValue;
}

void AppConfig::setString(const std::string& key, const std::string& value) {
    configMap[key] = value;
}

void AppConfig::setInt(const std::string& key, int value) {
    configMap[key] = std::to_string(value);
}

std::vector<std::string> AppConfig::getRecentUrls() const {
    return recentUrls;
}

void AppConfig::addRecentUrl(const std::string& url) {
    // 如果URL已经在列表中，先移除它
    auto it = std::find(recentUrls.begin(), recentUrls.end(), url);
    if (it != recentUrls.end()) {
        recentUrls.erase(it);
    }

    // 添加到列表前面
    recentUrls.insert(recentUrls.begin(), url);

    // 确保列表不超过最大大小
    if (recentUrls.size() > MAX_RECENT_URLS) {
        recentUrls.resize(MAX_RECENT_URLS);
    }
}
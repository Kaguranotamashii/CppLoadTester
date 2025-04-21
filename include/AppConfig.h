/**
* @file AppConfig.h
 * @brief 应用配置类的头文件
 */
#ifndef APP_CONFIG_H
#define APP_CONFIG_H

#include <string>
#include <map>
#include <vector>

/**
 * @brief 应用配置类（单例模式）
 */
class AppConfig {
public:
    // 获取单例实例
    static AppConfig& getInstance();

    // 禁止拷贝和赋值
    AppConfig(const AppConfig&) = delete;
    AppConfig& operator=(const AppConfig&) = delete;

    // 加载和保存配置
    bool loadFromFile(const std::string& filePath);
    bool saveToFile(const std::string& filePath);

    // 配置项访问函数
    std::string getString(const std::string& key, const std::string& defaultValue = "") const;
    int getInt(const std::string& key, int defaultValue = 0) const;
    void setString(const std::string& key, const std::string& value);
    void setInt(const std::string& key, int value);

    // 最近URL管理
    std::vector<std::string> getRecentUrls() const;
    void addRecentUrl(const std::string& url);

private:
    // 私有构造函数（单例模式）
    AppConfig();

    // 最大保存的最近URL数量
    static const int MAX_RECENT_URLS = 10;

    // 配置存储
    std::map<std::string, std::string> configMap;
    std::vector<std::string> recentUrls;
};

#endif // APP_CONFIG_H
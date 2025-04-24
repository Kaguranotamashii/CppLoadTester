/*  文件说明:
 *  1. FileHandle 负责处理文件相关的操作
 *  2. 包括文件的列表、上传、下载和删除等操作
 *  3. 提供获取文件列表页面和各种文件操作的方法
 */
#ifndef FILE_HANDLE_H
#define FILE_HANDLE_H

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

// 文件处理类,负责文件的列表、上传、下载和删除等操作
class FileHandle {
public:
    FileHandle();
    ~FileHandle();

    // 获取指定目录下的所有文件
    void getFileList(const std::string &dirName, std::vector<std::string> &fileList);
    
    // 构建文件列表页面
    void getFileListPage(std::string &fileListHtml);
    
    // 创建文件并写入数据
    bool createFile(const std::string &fileName, const char* data, size_t dataLen);
    
    // 追加数据到文件
    bool appendToFile(const std::string &fileName, const char* data, size_t dataLen);
    
    // 删除文件
    bool deleteFile(const std::string &fileName);
    
    // 打开文件并返回文件描述符
    int openFile(const std::string &fileName);
    
    // 获取文件大小
    long long getFileSize(int fd);
    
    // 关闭文件
    void closeFile(int fd);

private:
    // 处理文件上传的临时缓冲区大小
    static const int BUFFER_SIZE = 4096;
};

#endif // FILE_HANDLE_H
#include "file_handle.h"
#include "../utils/utils.h"

FileHandle::FileHandle() {
    // 构造函数实现
}

FileHandle::~FileHandle() {
    // 析构函数实现
}

void FileHandle::getFileList(const std::string &dirName, std::vector<std::string> &fileList) {
    // 使用 dirent 获取文件目录下的所有文件
    DIR *dir = opendir(dirName.c_str());
    if (!dir) {
        std::cout << outHead("error") << "打开目录 " << dirName << " 失败" << std::endl;
        return;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != nullptr) {
        std::string fileName = entry->d_name;
        // 排除 . 和 .. 目录
        if (fileName != "." && fileName != "..") {
            fileList.push_back(fileName);
        }
    }
    
    closedir(dir);
}

void FileHandle::getFileListPage(std::string &fileListHtml) {
    // 获取 filedir 目录下的所有文件
    std::vector<std::string> fileList;
    getFileList("filedir", fileList);
    
    // 构建页面
    std::ifstream fileListStream("html/filelist.html", std::ios::in);
    if (!fileListStream) {
        std::cout << outHead("error") << "打开文件列表模板文件失败" << std::endl;
        return;
    }

    std::string tempLine;
    // 首先读取文件列表的 <!--filelist_label--> 注释前的语句
    while (getline(fileListStream, tempLine)) {
        if (tempLine == "<!--filelist_label-->") {
            break;
        }
        fileListHtml += tempLine + "\n";
    }

    // 根据模板构建文件列表项
    for (const auto &fileName : fileList) {
        fileListHtml += "            <tr><td class=\"col1\">" + fileName +
                     "</td> <td class=\"col2\"><a href=\"download/" + fileName +
                     "\">下载</a></td> <td class=\"col3\"><a href=\"delete/" + fileName +
                     "\" onclick=\"return confirmDelete();\">删除</a></td></tr>" + "\n";
    }

    // 将文件列表注释后的语句加入后面
    while (getline(fileListStream, tempLine)) {
        fileListHtml += tempLine + "\n";
    }
    
    fileListStream.close();
}

bool FileHandle::createFile(const std::string &fileName, const char* data, size_t dataLen) {
    // 创建文件并写入数据
    std::ofstream ofs("filedir/" + fileName, std::ios::out | std::ios::binary);
    if (!ofs) {
        std::cout << outHead("error") << "创建文件 " << fileName << " 失败" << std::endl;
        return false;
    }
    
    ofs.write(data, dataLen);
    ofs.close();
    
    return true;
}

bool FileHandle::appendToFile(const std::string &fileName, const char* data, size_t dataLen) {
    // 追加数据到文件
    std::ofstream ofs("filedir/" + fileName, std::ios::out | std::ios::app | std::ios::binary);
    if (!ofs) {
        std::cout << outHead("error") << "打开文件 " << fileName << " 进行追加失败" << std::endl;
        return false;
    }
    
    ofs.write(data, dataLen);
    ofs.close();
    
    return true;
}

bool FileHandle::deleteFile(const std::string &fileName) {
    // 删除文件
    std::string filePath = "filedir/" + fileName;
    int ret = remove(filePath.c_str());
    
    if (ret != 0) {
        std::cout << outHead("error") << "删除文件 " << fileName << " 失败" << std::endl;
        return false;
    }
    
    std::cout << outHead("info") << "删除文件 " << fileName << " 成功" << std::endl;
    return true;
}

int FileHandle::openFile(const std::string &fileName) {
    // 打开文件并返回文件描述符
    std::string filePath = "filedir/" + fileName;
    int fd = open(filePath.c_str(), O_RDONLY);
    
    if (fd == -1) {
        std::cout << outHead("error") << "打开文件 " << fileName << " 失败" << std::endl;
    }
    
    return fd;
}

long long FileHandle::getFileSize(int fd) {
    // 获取文件大小
    struct stat fileStat;
    if (fstat(fd, &fileStat) == -1) {
        std::cout << outHead("error") << "获取文件大小失败" << std::endl;
        return -1;
    }
    
    return fileStat.st_size;
}

void FileHandle::closeFile(int fd) {
    // 关闭文件
    if (fd != -1) {
        close(fd);
    }
}
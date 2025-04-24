#include "http_processor.h"
#include <iostream>
#include <sstream>

// 构造函数:初始化 HttpProcessor,创建 FileHandler 实例,指定文件存储目录
HttpProcessor::HttpProcessor() : m_file_handler("filedir") {
    // 注意:filedir 目录需预先创建,否则文件保存会失败
}

// 处理 HTTP 请求
// 功能:根据请求状态(HANDLE_INIT, HANDLE_HEAD, HANDLE_BODY)分阶段解析请求
//       对于 POST 请求,解析 multipart/form-data,保存文件
//       对于 GET 请求,设置响应资源路径
// 参数:client_fd(客户端套接字,用于日志),request(请求数据),response(响应数据)
// 上传流程:在用户点击"上传文件"后,解析 POST 请求,提取文件名,调用 FileHandler 保存文件
void HttpProcessor::processRequest(int client_fd, Request& request, Response& response) {
    // 循环处理接收到的数据,直到缓冲区为空或状态机退出
    while (!request.recv_msg.empty()) {
        // 状态 1:初始状态,解析请求行
        if (request.status == HANDLE_INIT) {
            parseRequestLine(request);
            // 如果请求行未解析完成(数据不足),退出循环,等待更多数据
            if (request.status != HANDLE_HEAD) break;
        }
        // 状态 2:解析请求首部
        if (request.status == HANDLE_HEAD) {
            parseHeaders(request);
            // 如果首部未解析完成,退出循环
            if (request.status != HANDLE_BODY) break;
        }
        // 状态 3:处理 POST 请求的消息体(文件上传)
        if (request.status == HANDLE_BODY && request.request_method == "POST") {
            parseFileBody(request, response);
            // 如果文件上传未完成,退出循环
            if (request.status != HADNLE_COMPLATE) break;
        }
        // 状态 4:处理 GET 请求,设置响应资源路径
        if (request.status == HANDLE_BODY && request.request_method == "GET") {
            response.body_file_name = request.request_resource;
            request.status = HADNLE_COMPLATE;
            break;
        }
    }
    std::cout << "Processed request for client " << client_fd << ", status: " << request.status << std::endl;
}

// 解析请求行
// 功能:从 recv_msg 提取请求方法、资源路径、HTTP 版本,更新状态为 HANDLE_HEAD
// 示例:输入 "POST /upload HTTP/1.1\r\n",输出 request_method="POST", request_resource="/upload"
// 上传流程:解析 POST /upload,确认是文件上传请求
void HttpProcessor::parseRequestLine(Request& request) {
    // 查找请求行结束标志 \r\n
    std::string::size_type end_index = request.recv_msg.find("\r\n");
    if (end_index == std::string::npos) {
        // 数据不足,等待下次接收
        return;
    }

    // 提取请求行(如 "POST /upload HTTP/1.1\r\n")
    std::string line = request.recv_msg.substr(0, end_index + 2);
    request.recv_msg.erase(0, end_index + 2); // 删除已解析数据

    // 使用 stringstream 解析请求行
    std::istringstream iss(line);
    iss >> request.request_method >> request.request_resource >> request.http_version;

    // 更新状态为解析首部
    request.status = HANDLE_HEAD;
    std::cout << "Parsed request line: " << request.request_method << " " 
              << request.request_resource << " " << request.http_version << std::endl;
}

// 解析请求首部
// 功能:逐行解析首部字段,存储到 msg_header,遇到空行进入 HANDLE_BODY
// 示例:输入 "Content-Type: multipart/form-data; boundary=abc\r\n\r\n"
//       输出 msg_header["Content-Type"] = "multipart/form-data; boundary=abc"
// 上传流程:提取 Content-Type 和 boundary,准备解析文件数据
void HttpProcessor::parseHeaders(Request& request) {
    while (!request.recv_msg.empty()) {
        // 查找首部行结束标志 \r\n
        std::string::size_type end_index = request.recv_msg.find("\r\n");
        if (end_index == std::string::npos) {
            // 数据不足,退出循环
            break;
        }

        // 提取一行首部
        std::string line = request.recv_msg.substr(0, end_index + 2);
        request.recv_msg.erase(0, end_index + 2);

        // 遇到空行,表示首部解析完成
        if (line == "\r\n") {
            request.status = HANDLE_BODY;
            // 检查是否为文件上传请求
            if (request.msg_header["Content-Type"].find("multipart/form-data") != std::string::npos) {
                request.file_msg_status = FILE_BEGIN_FLAG;
                std::cout << "Detected multipart/form-data, preparing to parse file" << std::endl;
            }
            std::cout << "Parsed headers complete" << std::endl;
            break;
        }

        // 解析首部字段(如 "Content-Type: multipart/form-data; boundary=abc")
        std::string::size_type colon = line.find(": ");
        if (colon != std::string::npos) {
            std::string key = line.substr(0, colon);
            std::string value = line.substr(colon + 2, line.size() - colon - 4);
            request.msg_header[key] = value;
        }
    }
}

// 解析文件上传消息体(multipart/form-data)
// 功能:根据 boundary 解析文件数据,提取文件名,调用 FileHandler 保存文件,设置重定向
// 上传流程:核心函数,处理文件上传的 boundary、文件名、文件内容
void HttpProcessor::parseFileBody(Request& request, Response& response) {
    // 从首部提取 boundary(如 "----WebKitFormBoundary123")
    std::string boundary = request.msg_header["boundary"];
    if (boundary.empty()) {
        // 无 boundary,可能是无效请求,设置重定向
        response.body_file_name = "/redirect";
        request.status = HADNLE_COMPLATE;
        std::cout << "No boundary found, redirecting" << std::endl;
        return;
    }

    while (!request.recv_msg.empty()) {
        // 状态 1:查找文件开始边界(--boundary)
        if (request.file_msg_status == FILE_BEGIN_FLAG) {
            std::string::size_type end_index = request.recv_msg.find("\r\n");
            if (end_index == std::string::npos) {
                // 数据不足,等待下次接收
                break;
            }
            std::string flag = request.recv_msg.substr(0, end_index);
            if (flag == "--" + boundary) {
                // 找到开始边界,进入解析文件头部
                request.file_msg_status = FILE_HEAD;
                request.recv_msg.erase(0, end_index + 2);
                std::cout << "Found file boundary: --" << boundary << std::endl;
            } else {
                // 边界不匹配,设置重定向
                response.body_file_name = "/redirect";
                request.status = HADNLE_COMPLATE;
                std::cout << "Invalid boundary, redirecting" << std::endl;
                break;
            }
        }
        // 状态 2:解析文件头部(如文件名)
        else if (request.file_msg_status == FILE_HEAD) {
            std::string::size_type end_index = request.recv_msg.find("\r\n");
            if (end_index == std::string::npos) {
                // 数据不足
                break;
            }
            std::string line = request.recv_msg.substr(0, end_index + 2);
            request.recv_msg.erase(0, end_index + 2);

            // 遇到空行,表示文件头部解析完成,进入接收文件内容
            if (line == "\r\n") {
                request.file_msg_status = FILE_CONTENT;
                std::cout << "Parsed file headers, starting file content" << std::endl;
                continue;
            }

            // 查找文件名(如 Content-Disposition: form-data; name="upload"; filename="myfile.txt")
            end_index = line.find("filename=\"");
            if (end_index != std::string::npos) {
                line.erase(0, end_index + 10);
                request.recv_file_name = line.substr(0, line.find("\""));
                std::cout << "Found filename: " << request.recv_file_name << std::endl;
            }
        }
        // 状态 3:接收并保存文件内容
        else if (request.file_msg_status == FILE_CONTENT) {
            // 查找文件结束边界(\r\n--boundary--\r\n)
            std::string::size_type end_index = request.recv_msg.find("\r\n--" + boundary + "--\r\n");
            std::string::size_type save_len = request.recv_msg.size();

            // 找到结束边界,截取文件内容
            if (end_index != std::string::npos) {
                save_len = end_index;
                request.file_msg_status = FILE_COMPLATE;
            }

            // 保存文件内容
            if (save_len > 0) {
                m_file_handler.saveFile("filedir/" + request.recv_file_name,
                                       request.recv_msg.substr(0, save_len));
                request.recv_msg.erase(0, save_len);
                std::cout << "Saved " << save_len << " bytes to " << request.recv_file_name << std::endl;
            }

            // 文件上传完成,设置重定向响应
            if (request.file_msg_status == FILE_COMPLATE) {
                response.body_file_name = "/redirect";
                request.status = HADNLE_COMPLATE;
                std::cout << "File upload complete, redirecting to /" << std::endl;
                break;
            }
        }
    }
}

// 处理 HTTP 响应
// 功能:根据响应状态(HANDLE_INIT)调用 buildResponse 生成响应内容
// 参数:client_fd(客户端套接字),response(响应数据)
// 上传流程:文件上传完成后,生成 302 重定向响应,跳转到文件列表
void HttpProcessor::processResponse(int client_fd, Response& response) {
    if (response.status == HANDLE_INIT) {
        buildResponse(client_fd, response);
    }
}

// 构建响应
// 功能:根据资源路径(/、/download、/delete、/redirect)生成状态行、首部、消息体
// 示例:/ -> 文件列表页面,/download/filename -> 文件下载,/redirect -> 302 重定向
// 上传流程:生成 302 响应(Location: /),通知浏览器刷新文件列表
void HttpProcessor::buildResponse(int client_fd, Response& response) {
    std::string opera, filename;
    // 解析资源路径
    if (response.body_file_name == "/") {
        opera = "/"; // 文件列表
    } else {
        size_t pos = response.body_file_name.find('/', 1);
        if (pos != std::string::npos && pos < response.body_file_name.size() - 1) {
            opera = response.body_file_name.substr(1, pos - 1); // 如 download、delete
            filename = response.body_file_name.substr(pos + 1); // 文件名
        } else {
            opera = "redirect"; // 其他情况重定向
        }
    }

    // 处理文件列表请求
    if (opera == "/") {
        response.before_body_msg = buildStatusLine("HTTP/1.1", "200", "OK");
        m_file_handler.getFileListPage(response.msg_body); // 生成文件列表 HTML
        response.msg_body_len = response.msg_body.size();
        response.before_body_msg += buildHeaders(std::to_string(response.msg_body_len), "html");
        response.before_body_msg += "\r\n";
        response.before_body_msg_len = response.before_body_msg.size();
        response.body_type = HTML_TYPE;
        response.status = HANDLE_HEAD;
        std::cout << "Built file list response for client " << client_fd << std::endl;
    }
    // 处理文件下载请求
    else if (opera == "download") {
        response.before_body_msg = buildStatusLine("HTTP/1.1", "200", "OK");
        response.file_msg_fd = m_file_handler.openFile("filedir/" + filename);
        if (response.file_msg_fd == -1) {
            // 文件打开失败,重定向
            response = Response();
            response.body_file_name = "/redirect";
            std::cout << "Failed to open file " << filename << ", redirecting" << std::endl;
            return;
        }
        struct stat file_stat;
        fstat(response.file_msg_fd, &file_stat);
        response.msg_body_len = file_stat.st_size;
        response.before_body_msg += buildHeaders(std::to_string(response.msg_body_len), "file",
                                               "", std::to_string(response.msg_body_len - 1));
        response.before_body_msg += "\r\n";
        response.before_body_msg_len = response.before_body_msg.size();
        response.body_type = FILE_TYPE;
        response.status = HANDLE_HEAD;
        std::cout << "Built download response for " << filename << std::endl;
    }
    // 处理文件删除请求
    else if (opera == "delete") {
        bool success = m_file_handler.deleteFile("filedir/" + filename);
        std::cout << "Delete " << filename << (success ? " succeeded" : " failed") << std::endl;
        response = Response();
        response.body_file_name = "/redirect"; // 删除后重定向
    }
    // 处理重定向请求
    else {
        response.before_body_msg = buildStatusLine("HTTP/1.1", "302", "Moved Temporarily");
        response.before_body_msg += buildHeaders("0", "html", "/");
        response.before_body_msg += "\r\n";
        response.before_body_msg_len = response.before_body_msg.size();
        response.body_type = EMPTY_TYPE;
        response.status = HANDLE_HEAD;
        std::cout << "Built redirect response for client " << client_fd << std::endl;
    }
}

// 构建状态行
// 功能:生成 HTTP 状态行,如 "HTTP/1.1 200 OK\r\n"
// 参数:http_version(如 HTTP/1.1),status_code(如 200),status_des(如 OK)
// 返回:状态行字符串
// 上传流程:用于生成 302 重定向的状态行
std::string HttpProcessor::buildStatusLine(const std::string& http_version, const std::string& status_code, const std::string& status_des) {
    return http_version + " " + status_code + " " + status_des + "\r\n";
}

// 构建响应首部
// 功能:生成响应首部字段,如 Content-Length、Content-Type、Location
// 参数:content_length(消息体长度),content_type(html 或 file),redirect_location(重定向地址),content_range(文件范围)
// 返回:首部字符串
// 上传流程:生成 302 响应的首部(Content-Length: 0, Location: /)
std::string HttpProcessor::buildHeaders(const std::string& content_length, const std::string& content_type,
                                       const std::string& redirect_location, const std::string& content_range) {
    std::string header;
    if (!content_length.empty()) {
        header += "Content-Length: " + content_length + "\r\n";
    }
    if (content_type == "html") {
        header += "Content-Type: text/html;charset=UTF-8\r\n";
    } else if (content_type == "file") {
        header += "Content-Type: application/octet-stream\r\n";
    }
    if (!redirect_location.empty()) {
        header += "Location: " + redirect_location + "\r\n";
    }
    if (!content_range.empty()) {
        header += "Content-Range: 0-" + content_range + "\r\n";
    }
    header += "Connection: keep-alive\r\n";
    return header;
}
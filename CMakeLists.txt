cmake_minimum_required(VERSION 3.10)
project(CppLoadTester)

# 设置C++标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 设置Windows应用程序选项（根据编译器不同采用不同设置）
if(MSVC)
    # Visual Studio编译器
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /SUBSYSTEM:WINDOWS")
else()
    # MinGW/GCC编译器
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -mwindows")
endif()

# 添加预处理器定义
add_definitions(-DUNICODE -D_UNICODE)

# 添加源文件
set(SOURCES
        src/main.cpp
        src/LoadTester.cpp
        src/AppConfig.cpp
        src/UIManager.cpp
)

# 添加头文件
set(HEADERS
        include/LoadTester.h
        include/AppConfig.h
        include/UIManager.h
        include/StringConversion.h
)

# 添加包含目录
include_directories(include)

# 查找curl库 - 使用vcpkg推荐的方式
find_package(CURL CONFIG REQUIRED)
if(TARGET CURL::libcurl)
    message(STATUS "CURL找到并将使用CURL::libcurl目标链接。")
else()
    message(FATAL_ERROR "未找到CURL。请检查您的vcpkg安装。")
endif()

# 添加可执行文件
if(WIN32)
    add_executable(CppLoadTester WIN32 ${SOURCES} ${HEADERS})
else()
    add_executable(CppLoadTester ${SOURCES} ${HEADERS})
endif()

# 链接库 - 使用目标链接方式
target_link_libraries(CppLoadTester PRIVATE CURL::libcurl)

# 如果是Windows，还需链接其他库
if(WIN32)
    target_link_libraries(CppLoadTester PRIVATE wsock32 ws2_32 comctl32 shlwapi)
endif()

# 添加编译选项
if(MSVC)
    target_compile_options(CppLoadTester PRIVATE /W4)
else()
    target_compile_options(CppLoadTester PRIVATE -Wall -Wextra -pedantic)
endif()

# 设置输出目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
if(MINGW)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-libgcc -static-libstdc++")
endif()
# 安装指令
install(TARGETS CppLoadTester DESTINATION bin)

# 输出配置信息
message(STATUS "配置信息:")
message(STATUS "  生成器: ${CMAKE_GENERATOR}")
message(STATUS "  构建类型: ${CMAKE_BUILD_TYPE}")
message(STATUS "  CURL库: 已找到并将使用CURL::libcurl目标")
message(STATUS "  vcpkg工具链: ${CMAKE_TOOLCHAIN_FILE}")
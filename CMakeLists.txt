cmake_minimum_required(VERSION 3.10)
project(GameServerFramework VERSION 1.0.0 LANGUAGES CXX)

# ========== 编译选项 ==========
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 编译输出目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# ========== 编译标志 ==========
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_options(-g -Wall -Wextra)
    message(STATUS "Build Type: Debug")
else()
    add_compile_options(-O2 -Wall)
    message(STATUS "Build Type: Release")
endif()

# ========== 头文件目录 ==========
include_directories(
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/include/network
    ${PROJECT_SOURCE_DIR}/include/common
    ${PROJECT_SOURCE_DIR}/include/server
)

# ========== 源文件 ==========
set(NETWORK_SOURCES
    src/network/Socket.cpp
    src/network/Epoll.cpp
)

set(COMMON_SOURCES
    # src/common/Buffer.cpp  # 如果需要单独实现
)

set(SERVER_SOURCES
    # src/server/GameServer.cpp
    # src/server/Session.cpp
)

# ========== 静态库：网络模块 ==========
add_library(network STATIC ${NETWORK_SOURCES})

# ========== 可执行文件：主程序 ==========
add_executable(game_server
    src/main.cpp
    ${SERVER_SOURCES}
)

target_link_libraries(game_server
    network
    pthread  # Linux线程库
)

# ========== 示例程序 ==========
# add_executable(echo_server examples/echo_server.cpp)
# target_link_libraries(echo_server network pthread)

# ========== 测试程序 ==========
# enable_testing()
# add_executable(test_socket tests/test_socket.cpp)
# target_link_libraries(test_socket network pthread)
# add_test(NAME SocketTest COMMAND test_socket)

# ========== 安装规则 ==========
install(TARGETS game_server DESTINATION bin)
install(DIRECTORY include/ DESTINATION include)
install(FILES config/server.conf DESTINATION etc)

# ========== 打印配置信息 ==========
message(STATUS "========================================")
message(STATUS "Project: ${PROJECT_NAME} v${PROJECT_VERSION}")
message(STATUS "CMake Version: ${CMAKE_VERSION}")
message(STATUS "C++ Standard: C++${CMAKE_CXX_STANDARD}")
message(STATUS "Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "Install Prefix: ${CMAKE_INSTALL_PREFIX}")
message(STATUS "========================================")

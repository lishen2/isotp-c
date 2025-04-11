cmake_minimum_required(VERSION 3.10)

# 项目定义
project(isotp LANGUAGES C CXX)

# 编译选项
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 11)
if(MSVC)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /W3")
else()
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall")
endif()

# 构建isotp静态库
add_library(isotp STATIC
    isotp.c
    isotp.h
    isotp_defines.h
    isotp_user.h
)

# 安装配置
install(TARGETS isotp
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    PUBLIC_HEADER DESTINATION include
)

# 测试程序
set(CMAKE_CXX_STANDARD 11)
find_package(Threads REQUIRED)
add_executable(isotp_test
    isotp_test.cpp
)
target_link_libraries(isotp_test isotp Threads::Threads)

# 添加测试目标
enable_testing()
add_test(NAME isotp_test COMMAND isotp_test)

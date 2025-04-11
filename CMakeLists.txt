cmake_minimum_required(VERSION 3.10)

# 项目定义
project(isotp LANGUAGES C CXX)

# 编译选项
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 11)
if(MSVC)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /W3 /utf-8 /D_CRT_SECURE_NO_WARNINGS")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W3 /utf-8 /D_CRT_SECURE_NO_WARNINGS")
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

# 测试配置
set(CMAKE_CXX_STANDARD 11)
find_package(Threads REQUIRED)

# 功能测试程序(演示用)
add_executable(isotp_test isotp_test.cpp)
target_link_libraries(isotp_test isotp Threads::Threads)

# 单元测试配置
include(CTest)
enable_testing()

# 单元测试可执行文件
add_executable(test_isotp test_isotp.cpp isotp.c isotp_user_impl.c)
target_link_libraries(test_isotp isotp Threads::Threads)

# 添加单元测试
add_test(NAME test_isotp 
    COMMAND ${CMAKE_CURRENT_BINARY_DIR}/test_isotp
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

# 设置测试属性
set_tests_properties(test_isotp
    PROPERTIES
    TIMEOUT 30
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

cmake_minimum_required(VERSION 3.10)

# Project definition
project(isotp LANGUAGES C)

# Get all include directories
include_directories(src)
# Compile isotp as Shared Lib
add_library(isotp SHARED isotp.c)

target_compile_options(isotp PUBLIC -fPIC)
target_include_directories(isotp PUBLIC src)
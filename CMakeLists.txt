cmake_minimum_required(VERSION 3.10)

###
# Project definition
###
project(isotp LANGUAGES C VERSION 1.0.1 DESCRIPTION "A platform-agnostic ISOTP implementation in C for embedded devices.")

option(isotpc_USE_INCLUDE_DIR "Copy header files to separate include directory in current binary dir for better separation of header files to combat potential naming conflicts." OFF)
option(isotpc_STATIC_LIBRARY "Compile libisotpc as a static library, instead of a shared library." OFF)

if (isotpc_STATIC_LIBRARY)
    add_library(isotp STATIC ${CMAKE_CURRENT_SOURCE_DIR}/isotp.c)
else()
    add_library(isotp SHARED ${CMAKE_CURRENT_SOURCE_DIR}/isotp.c)
endif()

###
# Strict building policies
###
target_compile_options(
    isotp PUBLIC
    -fPIC
    -Werror
    -Wall
    -Wno-unknown-pragmas # ignore unknown pragmas, such as #pragma region
)

###
# Check for debug builds
###
if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(isotp PUBLIC -O0 -g) # don't optimise and add debugging symbols
else()
    target_compile_options(isotp PUBLIC -O2) # optimise code
    target_link_libraries(isotp PUBLIC -s) # strip the library
endif()

if (isotpc_USE_INCLUDE_DIR)
    file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/include/isotp_c)
    
    file(GLOB HEADERS FOLLOW_SYMLINKS ${CMAKE_CURRENT_SOURCE_DIR}/*.h)
    file(COPY ${HEADERS} DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/include/isotp_c/)

    target_include_directories(isotp PUBLIC ${CMAKE_CURRENT_BINARY_DIR}/include)

else()
    target_include_directories(isotp PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
endif()

###
# Add library aliases
###
add_library(isotpc ALIAS isotp)
add_library(isotp_c ALIAS isotp)
add_library(simon_cahill::isotp ALIAS isotp)
add_library(simon_cahill::isotpc ALIAS isotp)
add_library(simon_cahill::isotp_c ALIAS isotp)

cmake_minimum_required(VERSION 3.10)

###
# Project definition
###
project(isotp LANGUAGES C VERSION 1.0.1 DESCRIPTION "A platform-agnostic ISOTP implementation in C for embedded devices.")

option(isotpc_USE_INCLUDE_DIR "Copy header files to separate include directory in current binary dir for better separation of header files to combat potential naming conflicts." OFF)
option(isotpc_STATIC_LIBRARY "Compile libisotpc as a static library, instead of a shared library." OFF)
option(isotpc_STATIC_LIBRARY_PIC "Make use of position independent code (PIC), when compiling as a static library (enabled automatically when building shared libraries)." OFF)
option(isotpc_PAD_CAN_FRAMES "Pad CAN frames to their full size." ON)
set(isotpc_CAN_FRAME_PAD_VALUE "0xAA" CACHE STRING "Padding byte value to be used in CAN frames if enabled")

if (isotpc_STATIC_LIBRARY)
    add_library(isotp STATIC ${CMAKE_CURRENT_SOURCE_DIR}/isotp.c)
else()
    add_library(isotp SHARED ${CMAKE_CURRENT_SOURCE_DIR}/isotp.c)
endif()

###
# Strict building policies
###
target_compile_options(
    isotp PRIVATE
    -Werror
    -Wall
    -Wno-unknown-pragmas # ignore unknown pragmas, such as #pragma region
)

###
# Enable position independent code (PIC) if required
###
if (NOT isotpc_STATIC_LIBRARY OR isotpc_STATIC_LIBRARY_PIC)
    # Building a shared library or a static library with PIC enabled
    target_compile_options(isotp PRIVATE -fPIC)
endif()

###
# Provide padding configuration
###
if (isotpc_PAD_CAN_FRAMES)
    target_compile_definitions(isotp PRIVATE -DISO_TP_FRAME_PADDING -DISO_TP_FRAME_PADDING_VALUE=${isotpc_CAN_FRAME_PAD_VALUE})
endif()

###
# Check for debug builds
###
if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(isotp PRIVATE -O0 -g) # don't optimise and add debugging symbols
else()
    target_compile_options(isotp PRIVATE -O2) # optimise code
    target_link_libraries(isotp PRIVATE -s) # strip the library
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

cmake_minimum_required(VERSION 3.12)

# Set C++ standard to C++17
set(CMAKE_CXX_STANDARD 17)

project("Metin2 Server")

set(BINARY_OUTPUT_DIR ${CMAKE_BINARY_DIR}/bin)

# Set build directory
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Set the CMake module directory
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules/")

# Set environment variables
set(METIN2_OS_NAME ${CMAKE_SYSTEM})
set(METIN2_COMPILER "${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
set(METIN2_CPU_TARGET ${CMAKE_SYSTEM_PROCESSOR})

# Git revision
find_package(GitInfo)

if(GIT_FOUND)
    GIT_WC_INFO(${PROJECT_SOURCE_DIR} Metin2)
    set(METIN2_REVISION ${Metin2_WC_REVISION_HASH})
    set(METIN2_LAST_CHANGED_DATE ${Metin2_WC_LAST_CHANGED_DATE})

    if (Metin2_WC_LATEST_TAG)
        set(METIN2_LATEST_TAG ${Metin2_WC_LATEST_TAG})
    else()
        set(METIN2_LATEST_TAG "unknown")
    endif()
else()
    set(METIN2_REVISION "unknown")
    set(METIN2_LAST_CHANGED_DATE "\"unknown\"")
    set(METIN2_LATEST_TAG "unknown")
endif()

message(STATUS "Current revision is ${METIN2_REVISION}")

# Generate the version header
configure_file(
    "src/common/version.h.in"
    "${PROJECT_BINARY_DIR}/common/version.h"
)
include_directories(${PROJECT_BINARY_DIR}/common/)

# Set the global include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src/)

add_subdirectory(src/libgame)
add_subdirectory(src/liblua)
add_subdirectory(src/libpoly)
add_subdirectory(src/libsql)
add_subdirectory(src/libthecore)
add_subdirectory(src/game)
add_subdirectory(src/db)
add_subdirectory(src/quest)

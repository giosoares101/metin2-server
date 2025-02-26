cmake_minimum_required(VERSION 3.8)

project(db CXX)

file(GLOB_RECURSE sources
    src/*.cpp src/*.h
)

# Add the src directory to the include path
include_directories(src)

add_executable(${PROJECT_NAME} ${sources})

# Set the default log level based on the build type
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    message(STATUS "This is a debug build. Log level will be set to 'trace' for target '${PROJECT_NAME}'.")
    target_compile_definitions(${PROJECT_NAME} PRIVATE SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_TRACE)
endif()

# Treat char variables as signed, especially useful for ARM builds
target_compile_options(${PROJECT_NAME} PUBLIC -fsigned-char)

# Find and link dependencies

#
# vcpkg dependencies
#

# Libevent
find_package(Libevent CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} libevent::core libevent::extra libevent::pthreads)

# effolkronium/random
find_package(effolkronium_random CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} effolkronium_random)

#
# System-provided dependencies
#

# Pthreads
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
target_link_libraries(${PROJECT_NAME} Threads::Threads)

# LibBSD
target_link_libraries(${PROJECT_NAME} bsd)

target_link_libraries(${PROJECT_NAME} libpoly libsql libthecore)

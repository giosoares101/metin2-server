cmake_minimum_required(VERSION 3.8)

project(libsql CXX)

# Find source files
file(GLOB SOURCES
	"src/*.cpp"
	"include/*.h"
)

# Include header files
include_directories("include")

# Create shared library
add_library(${PROJECT_NAME} STATIC ${SOURCES})

# Find and link dependencies

# MariaDB
find_package(unofficial-libmariadb REQUIRED)
target_link_libraries(${PROJECT_NAME} PUBLIC unofficial::libmariadb)

# spdlog
find_package(spdlog CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE spdlog::spdlog)

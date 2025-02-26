cmake_minimum_required(VERSION 3.8)

project(libpoly CXX)

# Find source files
file(GLOB SOURCES
	"src/*.cc"
	"include/*.h"
)

# Include header files
include_directories("include")

# Create shared library
add_library(${PROJECT_NAME} STATIC ${SOURCES})
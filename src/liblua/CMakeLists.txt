cmake_minimum_required(VERSION 3.8)

project(liblua CXX)

# Find source files
file(GLOB_RECURSE SOURCES
	"src/*.c"
	"src/*.h"
	"include/*.h"
)

# Include header files
include_directories(
	"include"
	"src"
)

# Set the language to C++ on Visual Studio
if (MSVC)
	SET_SOURCE_FILES_PROPERTIES(${SOURCES} PROPERTIES LANGUAGE CXX)
endif (MSVC)

# Create shared library
add_library(${PROJECT_NAME} STATIC ${SOURCES})
target_include_directories(${PROJECT_NAME} PUBLIC 
	"include"
	"src"
)
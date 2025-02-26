cmake_minimum_required(VERSION 3.8)

project(quest CXX)

file(GLOB_RECURSE sources
  src/*.cc src/*.h
  src/*.c
)

# Add the src directory to the include path
include_directories(src)

add_executable(${PROJECT_NAME} ${sources})
set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME "qc")

# Find and link dependencies

# liblua
target_link_libraries(${PROJECT_NAME} liblua)

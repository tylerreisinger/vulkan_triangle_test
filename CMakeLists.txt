cmake_minimum_required(VERSION 2.6)
project(vulkan_landscape)

add_subdirectory(${PROJECT_SOURCE_DIR}/src)
include_directories("src")

if(NOT MSVC)
    list(APPEND CMAKE_CXX_FLAGS "-std=c++17")
endif()

add_executable(landscape ${SOURCES})

target_link_libraries(landscape glfw vulkan)


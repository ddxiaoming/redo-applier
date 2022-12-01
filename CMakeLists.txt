cmake_minimum_required(VERSION 3.16)
set(CMAKE_CXX_STANDARD 17)
project(REDO_APPLIER)
enable_language(C CXX ASM)

include_directories(${PROJECT_SOURCE_DIR}/include)
add_subdirectory(applier)

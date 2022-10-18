cmake_minimum_required(VERSION 3.16)
project(REDO_APPLIER)
set(CMAKE_CXX_STANDARD 14)

set(SOURCE_FILE
        ${PROJECT_SOURCE_DIR}/src/main.cpp
        ${PROJECT_SOURCE_DIR}/src/apply/apply.cpp
        ${PROJECT_SOURCE_DIR}/src/apply/parse.cpp
        ${PROJECT_SOURCE_DIR}/src/page/page.cpp
        ${PROJECT_SOURCE_DIR}/src/page/page_layout.cpp
        ${PROJECT_SOURCE_DIR}/src/utility/utility.cpp
        ${PROJECT_SOURCE_DIR}/src/buffer/buffer_pool.cpp
        ${PROJECT_SOURCE_DIR}/src/bean/bean.cpp
        ${PROJECT_SOURCE_DIR}/src/record/record.cpp
        )
add_executable(Applier ${SOURCE_FILE})
target_include_directories(Applier PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(Applier fmt)
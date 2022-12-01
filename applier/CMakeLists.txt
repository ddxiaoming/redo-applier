project(applier)
set(CMAKE_CXX_STANDARD 17)
set(APPLIER_SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/src/apply/apply.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/apply/parse.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/bean/bean.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/buffer/buffer_pool.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/record/record.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/utility/utility.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/utility/file_io.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp)

# make shared library
add_library(applier SHARED)
target_sources(applier PRIVATE ${APPLIER_SOURCE})
target_link_options(applier PRIVATE -nostdlib)
target_compile_options(applier PRIVATE -fPIC)
target_link_directories(applier PRIVATE ${CMAKE_SOURCE_DIR}/libstorpu/obj)
target_link_libraries(applier supc++ storpu) # linking freestanding c++ and libstorpu.a
if (${CMAKE_BUILD_TYPE} STREQUAL "Debug")
    target_compile_definitions(applier PRIVATE DEBUG)
endif ()

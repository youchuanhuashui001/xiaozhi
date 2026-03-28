cmake_minimum_required(VERSION 3.20)
project(xiaozhi C CXX)

enable_testing()

file(GLOB CORE_SRCS CONFIGURE_DEPENDS
    src/*.c
    src/*.cc
)

add_library(xiaozhi_core STATIC ${CORE_SRCS})

add_executable(xiaozhi_daemon src/daemon/main_daemon.c)
target_link_libraries(xiaozhi_daemon PRIVATE xiaozhi_core)

add_subdirectory(gui EXCLUDE_FROM_ALL)

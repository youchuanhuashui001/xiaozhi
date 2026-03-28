cmake_minimum_required(VERSION 3.20)
project(xiaozhi C CXX)

enable_testing()

include(cmake/Dependencies.cmake)

add_subdirectory(gui EXCLUDE_FROM_ALL)

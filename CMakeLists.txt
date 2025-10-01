cmake_minimum_required(VERSION 3.12)

project(goober CXX)

add_compile_options(-std=c++23)

set(GOOBER_SOURCES
    src/entrypoint.cpp
    src/lib/lib.cpp
    src/network/network.cpp
    src/java/java.cpp
    src/ipc/ipc.cpp)

set(GOOBER_HEADERS
    src/lib/lib.hpp
    src/network/network.hpp
    src/java/java.hpp
    src/ipc/ipc.hpp)

add_library(goober SHARED
    ${GOOBER_SOURCES}
    ${GOOBER_HEADERS})

string(TOLOWER "${CMAKE_SYSTEM_NAME}" GOOBER_OS)

# TODO: should probably depend on system java in the future?
target_include_directories(goober PRIVATE ext/java ext/java/${GOOBER_OS})

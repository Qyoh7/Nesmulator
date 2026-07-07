cmake_minimum_required(VERSION 3.31)

project(Nesmulator LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(Nesmulator
    src/main.cpp
)

target_include_directories(Nesmulator PRIVATE include)

target_compile_options(Nesmulator PRIVATE
    -Wall
    -Wextra
    -Wpedantic
)

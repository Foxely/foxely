cmake_minimum_required(VERSION 3.16)

set(CMAKE_CXX_STANDARD 14)

project(Foxely VERSION 1.0.0 LANGUAGES CXX)

file(GLOB_RECURSE SRCS "src/*.cpp")

add_subdirectory(lib)

add_library(foxely STATIC ${SRCS})
target_include_directories(foxely PUBLIC "include")
target_link_libraries(foxely ${CMAKE_DL_LIBS} lexer)

# add_subdirectory(examples)
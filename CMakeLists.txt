cmake_minimum_required(VERSION 2.8)

project(reflectable_sample)

SET(CMAKE_CXX_FLAGS "-std=c++0x")

FIND_PACKAGE(Boost REQUIRED)

include_directories("${CMAKE_SOURCE_DIR}/inc/reflectable" ${Boost_INCLUDE_DIR})

add_executable(reflectable_sample "example/Source.cpp")

install (DIRECTORY "${CMAKE_SOURCE_DIR}/inc/" DESTINATION include)
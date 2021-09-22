cmake_minimum_required(VERSION 3.7)

project(VSGRayTracer)

set(CMAKE_CXX_STANDARD 17)

find_package(vsg REQUIRED)
find_package(vsgXchange REQUIRED)

add_executable(VSGRayTracer src/main.cpp)
target_link_libraries(VSGRayTracer vsg::vsg vsgXchange::vsgXchange)
cmake_minimum_required(VERSION 3.7)

project(VSGRayTracer)

set(CMAKE_CXX_STANDARD 17)

find_package(vsg REQUIRED)
find_package(vsgXchange REQUIRED)

add_executable(VSGRayTracer "src/main.cpp" "src/utils.cpp" "include/utils.h" "include/RayTracingUniform.h" "include/SceneConversionTraversal.h" "src/SceneConversionTraversal.cpp" "include/RayTracingMaterialGroup.h" "src/RayTracingMaterialGroup.cpp" "include/RayTracingVisitor.h" "include/RayTracingMaterial.h")
target_include_directories(VSGRayTracer PRIVATE "include/")
target_link_libraries(VSGRayTracer vsg::vsg vsgXchange::vsgXchange)
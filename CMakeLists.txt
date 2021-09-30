cmake_minimum_required(VERSION 3.7)

project(VSGRayTracer)

set(CMAKE_CXX_STANDARD 17)

find_package(vsg REQUIRED)
find_package(vsgXchange REQUIRED)
find_package(Vulkan REQUIRED)

add_executable(VSGRayTracer "src/main.cpp" "src/utils.cpp" "include/utils.h" "include/RayTracingUniform.h" "include/SceneConversionTraversal.h" "src/SceneConversionTraversal.cpp" "include/RayTracingMaterialGroup.h" "src/RayTracingMaterialGroup.cpp" "include/RayTracingVisitor.h" "include/RayTracingMaterial.h" "include/RayTracer.h" "src/RayTracer.cpp")
target_include_directories(VSGRayTracer PRIVATE "include/")
target_link_libraries(VSGRayTracer vsg::vsg vsgXchange::vsgXchange)

set(GLSLC_FLAGS "--target-env=vulkan1.1" "--target-spv=spv1.4")

function(add_shader SOURCE_FILE SPIRV_FILE)
  add_custom_command(
    OUTPUT ${SPIRV_FILE}
    COMMAND Vulkan::glslc ${GLSLC_FLAGS} -o ${SPIRV_FILE} ${CMAKE_CURRENT_SOURCE_DIR}/${SOURCE_FILE}
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${SOURCE_FILE}
    VERBATIM)
endfunction()

add_shader("shaders/miss.rmiss" "shaders/miss.spv")
add_shader("shaders/closestHit.rchit" "shaders/closestHit.spv")
add_shader("shaders/rayGeneration.rgen" "shaders/rayGeneration.spv")

add_custom_target(
  shaders ALL
  DEPENDS "shaders/miss.spv" "shaders/closestHit.spv" "shaders/rayGeneration.spv")

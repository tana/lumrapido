cmake_minimum_required(VERSION 3.7)

project(VSGRayTracer)

set(CMAKE_CXX_STANDARD 17)

find_package(vsg REQUIRED)
find_package(Vulkan REQUIRED)

set(TINYGLTF_HEADER_ONLY ON CACHE INTERNAL "" FORCE)
set(TINYGLTF_INSTALL OFF CACHE INTERNAL "" FORCE)
add_subdirectory("./tinygltf")

set(TINYEXR_BUILD_SAMPLE OFF CACHE INTERNAL "" FORCE)
add_subdirectory("./tinyexr")

add_executable(VSGRayTracer "src/main.cpp" "src/utils.cpp" "include/utils.h" "include/RayTracingUniform.h" "include/SceneConversionTraversal.h" "src/SceneConversionTraversal.cpp" "include/RayTracingMaterialGroup.h" "src/RayTracingMaterialGroup.cpp" "include/RayTracingVisitor.h" "include/RayTracingMaterial.h" "include/RayTracer.h" "src/RayTracer.cpp" "include/RayTracingScene.h" "src/RayTracingScene.cpp" "include/GLTFLoader.h" "src/GLTFLoader.cpp" "include/gltfUtils.h" "src/gltfUtils.cpp")
target_include_directories(VSGRayTracer PRIVATE "include/")
target_include_directories(VSGRayTracer PRIVATE "tinyexr/")
target_link_libraries(VSGRayTracer vsg::vsg tinygltf tinyexr)

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

cmake_minimum_required(VERSION 3.7)

project(lumrapido)

set(CMAKE_CXX_STANDARD 17)

find_package(vsg REQUIRED)
find_package(Vulkan REQUIRED)

set(TINYGLTF_HEADER_ONLY ON CACHE INTERNAL "" FORCE)
set(TINYGLTF_INSTALL OFF CACHE INTERNAL "" FORCE)
add_subdirectory("./tinygltf")

set(TINYEXR_BUILD_SAMPLE OFF CACHE INTERNAL "" FORCE)
add_subdirectory("./tinyexr")

add_executable(lumrapido "src/main.cpp" "src/utils.cpp" "include/utils.h" "include/RayTracingUniform.h" "include/SceneConversionTraversal.h" "src/SceneConversionTraversal.cpp" "include/RayTracingMaterialGroup.h" "src/RayTracingMaterialGroup.cpp" "include/RayTracingVisitor.h" "include/RayTracingMaterial.h" "include/RayTracer.h" "src/RayTracer.cpp" "include/RayTracingScene.h" "src/RayTracingScene.cpp" "include/GLTFLoader.h" "src/GLTFLoader.cpp" "include/gltfUtils.h" "src/gltfUtils.cpp" "include/hammersley.h" "src/hammersley.cpp"  "include/EnvMapSamplingData.h" "src/EnvMapSamplingData.cpp")
target_include_directories(lumrapido PRIVATE "include/")
target_include_directories(lumrapido PRIVATE "tinyexr/")
target_link_libraries(lumrapido vsg::vsg tinygltf tinyexr)

set(GLSLC_FLAGS "--target-env=vulkan1.1" "--target-spv=spv1.4")

set(SPIRV_HEADERS "${CMAKE_CURRENT_SOURCE_DIR}/shaders/common.glsl")

function(add_shader SPIRV_FILE SOURCE_FILE ADDITIONAL_FLAGS)
  add_custom_command(
    OUTPUT ${SPIRV_FILE}
    COMMAND Vulkan::glslc ${GLSLC_FLAGS} ${ADDITIONAL_FLAGS} -o ${SPIRV_FILE} ${CMAKE_CURRENT_SOURCE_DIR}/${SOURCE_FILE}
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${SOURCE_FILE} ${SPIRV_HEADERS}
    VERBATIM)
endfunction()

add_shader("shaders/miss.spv" "shaders/miss.rmiss" "")
add_shader("shaders/closestHit.spv" "shaders/closestHit.rchit" "")
add_shader("shaders/rayGeneration.spv" "shaders/rayGeneration.rgen" "-DALGORITHM_PATH_TRACING")
add_shader("shaders/rayGenerationQMC.spv" "shaders/rayGeneration.rgen" "-DALGORITHM_QUASI_MONTE_CARLO")

add_custom_target(
  shaders ALL
  DEPENDS "shaders/miss.spv" "shaders/closestHit.spv" "shaders/rayGeneration.spv" "shaders/rayGenerationQMC.spv")

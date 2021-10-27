# Lumrapido
A real-time ray tracer based on Vulkan Ray Tracing and [VulkanSceneGraph](https://github.com/vsg-dev/VulkanSceneGraph).

## :star: Features
- :volcano: **Hardware-accelerated ray tracing** using Vulkan Ray Tracing extension
- :bulb: Global illumination using **path tracing** algorithm
- :teapot: Model loading from **[glTF](https://github.com/KhronosGroup/glTF) format**
- :crystal_ball: **Physically-based materials**

## :rocket: Usage
### Building
Before building this program, [Vulkan SDK](https://www.lunarg.com/vulkan-sdk/) and [VulkanSceneGraph](https://github.com/vsg-dev/VulkanSceneGraph) have to be installed.

Lumrapido can be built using CMake:
```powershell
mkdir build
cd build
cmake .. -G "Visual Studio 16 2019" -A x64
cmake --build .
```

Alternatively, Visual Studio can be used to open and build a CMake project.

### Running
After building, `build/shaders` directory, which contains SPIR-V (`.spv`) files, have to be placed next to the executable file (`lumrapido.exe` on Windows).
```
lumrapido.exe
shaders/
  closestHit.spv
  miss.spv
  rayGeneration.spv
```

### Command line options
```
lumrapido [OPTIONS] GLTF_FILE
```
#### Options
- `-e EXR_FILE`: Specify equirectangular environment map (OpenEXR image) for image-based lighting.
- `-s SAMPLES_PER_PIXEL`: Set number of samples per pixel.
- `-c "X Y Z"`: Set initial camera position.
- `-l "X Y Z"`: Set initial target position of the camera.
- `-u "X Y Z"`: Set upward direction of the camera.
- `-f FOV`: Set horizontal field of view of the camera in degrees (default is 90 deg).
- `-W WIDTH`: Set window width.
- `-H HEIGHT`: Set window height.
- `-a ALGORITHM`: Choose sampling algorithm to use. Supported algorithms are:
  - `pt` Vanilla path tracing (default).
  - `qmc` Quasi-Monte Carlo algorithm using Hammersley sequence (:warning: **buggy**).
- `--debug`: Enable Vulkan validation layer (for debugging).


## :books: References
Lumrapido is based on VulkanSceneGraph's [`vsgraytracing` example](https://github.com/vsg-dev/vsgExamples/blob/master/examples/raytracing/vsgraytracing/vsgraytracing.cpp).

Also, algorithm of ray tracing and default scene are based on ["Ray Tracing in One Weekend"](https://raytracing.github.io/books/RayTracingInOneWeekend.html) (v3.2.3) written by Peter Shirley.

Vulkan-specific designs are based on [NVIDIA Vulkan Ray Tracing tutorial](https://nvpro-samples.github.io/vk_raytracing_tutorial_KHR/).

Algorithm of Quasi-Monte Carlo sampling is based on the book ["Physically Based Rendering: From Theory To Implementation"](https://www.pbr-book.org/3ed-2018/contents) written by Matt Pharr, Wenzel Jakob, and Greg Humphreys.

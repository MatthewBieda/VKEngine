VKEngine is a 3D rendering engine created with Vulkan and C++ 20.

It picks up where OGLRenderer left off.

**OGLRenderer Achievements:**

- Physically Based Rendering (PBR) with Cook-Torrance BRDF, full glTF 2.0 material/texture support
- Dynamic lighting & shadows: directional (sun), point, and spot lights with PCF shadow maps for sunlight
- Image Based Lighting (IBL): skyboxes, HDR environment maps, irradiance & prefiltered radiance, BRDF LUT
- Post-processing pipeline: HDR tone mapping, gamma correction, MSAA, exposure
- Scene editor: Free camera, Instanced model loading, runtime editing through ImGui interface

**Roadmap:**	

1. Achieve feature parity with OGLRenderer
2. Adopt Modern Vulkan features! Dynamic Rendering, Bindless Descriptors, Buffer Device Address (BDA), Timeline Semaphores

3. Then add the following features:
    - Deferred Rendering / Tiled Deferred
    - Cascading Shadow Maps
    - Bloom/SSAO/SSR
    - Skeletal Animations
    - Spotlight & Point Light Shadows (Omnidirectional)
    - Transparency
    - Frustum Culling / Occlusion Culling
    - Probe based Global Illumination

**Current Status:**

- Load a 3D model (Obj) with a diffuse texture map
- Instanced rendering allows any amount of duplicate meshes using 1 draw call
- Free camera & UI interaction modes with seamless toggle
- ImGui interface that allows editing of dynamic pipeline state
- Dynamic Lighting with Directional & Point Lights using Blinn-Phong shading
- Debugging functionality with labels/object names for RenderDoc/Nsight
- MSAA & Swapchain recreation

## Devlog 0
- Motivations & Dependencies

[![VKEngine - Devlog 0](https://img.youtube.com/vi/qB6mkcmTGvY/0.jpg)](https://www.youtube.com/watch?v=qB6mkcmTGvY)

## Devlog 1
- Buffers & Textures

[![VKEngine - Devlog 1](https://img.youtube.com/vi/XylJVviVezg/0.jpg)](https://www.youtube.com/watch?v=XylJVviVezg)

## Devlog 2
- Loading a 3D Model and implementing Depth Testing/Multisampling/MSAA

[![VKEngine - Devlog 2](https://img.youtube.com/vi/BNghrnk86vo/0.jpg)](https://www.youtube.com/watch?v=BNghrnk86vo)

## Devlog 3
- Debug Utils, ImGui integration, Dynamic pipeline state & Swapchain recreation

[![VKEngine - Devlog 3](https://img.youtube.com/vi/0DAru1Xl0Jc/0.jpg)](https://www.youtube.com/watch?v=0DAru1Xl0Jc)

## Devlog 4
- Camera system, Dynamic Lighting & Blinn-Phong Shading

[![VKEngine - Devlog 4](https://img.youtube.com/vi/oiAcDZiqOqE/0.jpg)](https://www.youtube.com/watch?v=oiAcDZiqOqE)
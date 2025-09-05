VKEngine is a 3D rendering engine created with Vulkan and C++.

It picks up where OGLRenderer left off.

**OGLRenderer Achievements:**

- Physically Based Rendering (PBR) with Cook-Torrance BRDF, full glTF 2.0 material/texture support
- Dynamic lighting & shadows: directional (sun), point, and spot lights with PCF shadow maps for sunlight
- Image Based Lighting (IBL): skyboxes, HDR environment maps, irradiance & prefiltered radiance, BRDF LUT
- Post-processing pipeline: HDR tone mapping, gamma correction, MSAA, exposure
- Scene editor: Free camera, Instanced model loading, runtime editing through ImGui interface

**Roadmap:**	

1. Achieve feature parity with OGLRenderer
2. Adopt Modern Vulkan features! Dynamic Rendering (already implemented), Bindless Descriptors, Buffer Device Address (BDA), Timeline Semaphores

3. Then add the following features:
    - Deferred Rendering / Tiled Deferred
    - Cascading Shadow Maps
    - Bloom/SSAO/SSR
    - Skeletal Animations
    - Spotlight & Point Light Shadows (Omnidirectional)
    - Transparency
    - Frustum Culling / Occlusion Culling
    - Probe based Global Illumination
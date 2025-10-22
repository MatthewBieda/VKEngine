<h1 align="center">VKEngine</h1>
<h3 align="center">A 3D rendering engine created with Vulkan and C++20.</h3>

![VKEngine Latest Version](https://i.ibb.co/VshfqQg/VKEngine-Episode6-Cover.jpg)

### Current Features:

- **3D Model and Texture Loading**  
  Load and render 3D models with associated textures.

- **Skybox Rendering**  
  Support for panoramic backgrounds and environment visuals.

- **Environment Mapping with Fresnel Reflections**  
  Realistic reflective materials that respond to viewing angles.

- **Transparency Support** *(Alpha Testing + Alpha Blending)*  
  Supports both cutout materials (e.g., grass, foliage) and blended transparency (e.g., glass, windows).

- **MSAA (Multisample Anti-Aliasing)**  
  Smooths jagged edges for improved visual quality.

- **Dynamic Lighting System (Directional / Point Lights)**  
  Real-time lighting with multiple light types.

- **Blinn-Phong Shading Model**  
  Classic, efficient lighting model for realistic highlights.

- **Fully Controllable 3D Camera**  
  Free movement and orientation for interactive scenes.

- **ImGui-Based Editor Interface**  
  Integrated in-engine editor for debugging and scene control.

- **Swapchain Recreation Handling**  
  Robust handling of window resize and minimization.

- **Mipmap Generation**  
  Automatic mipmap creation for texture quality and performance.

- **Instanced Rendering**  
  Efficiently render large numbers of identical objects.

- **Material Batching**  
  Batch draw calls on a per-material basis to reduce the number of state changes needed in the render loop.

- **Animations**  
  Update an object's model matrix at runtime to translate, rotate and scale any object. 

- **Light Casters**  
  A small cube mesh that tracks the positions of a point light, making placing lights easy.

- **Bindless Textures**  
  Uses descriptor indexing to give shaders direct access to large arrays of textures, eliminating the need to bind textures individually per draw call.

### Roadmap:	

- Normal Maps
- Frustum Culling
- Skeletal Animations
- Cascading Shadow Maps
- Deferred Rendering (Tiled Deferred + Forward Pass)

### Devlog 0
- Motivations & Dependencies

[![VKEngine - Devlog 0](https://img.youtube.com/vi/qB6mkcmTGvY/0.jpg)](https://www.youtube.com/watch?v=qB6mkcmTGvY)

### Devlog 1
- Buffers & Textures

[![VKEngine - Devlog 1](https://img.youtube.com/vi/XylJVviVezg/0.jpg)](https://www.youtube.com/watch?v=XylJVviVezg)

### Devlog 2
- Loading a 3D Model and implementing Depth Testing/Mipmaps/MSAA

[![VKEngine - Devlog 2](https://img.youtube.com/vi/BNghrnk86vo/0.jpg)](https://www.youtube.com/watch?v=BNghrnk86vo)

### Devlog 3
- Debug Utils, ImGui integration, Dynamic pipeline state & Swapchain recreation

[![VKEngine - Devlog 3](https://img.youtube.com/vi/0DAru1Xl0Jc/0.jpg)](https://www.youtube.com/watch?v=0DAru1Xl0Jc)

### Devlog 4
- Camera system, Dynamic Lighting & Blinn-Phong Shading

[![VKEngine - Devlog 4](https://img.youtube.com/vi/oiAcDZiqOqE/0.jpg)](https://www.youtube.com/watch?v=oiAcDZiqOqE)

### Devlog 5

**Major Features:**
- Skyboxes
- Environment mapping with Fresnel reflections
- Transparency support (alpha testing + alpha blending)
- Bindless texture system
- Dynamic buffer updates (no more duplicate buffers!)
- Per-mesh batched instancing

**Implementation Details:**
- Add Mesh struct to store vertex/index offsets and counts
- Create mesh SSBO for GPU-side mesh metadata storage
- Refactor ObjectData to reference mesh and texture indices
- Update loadModel() to append geometry to shared vertex/index buffers
- Group draw calls by mesh type for efficient batched rendering
- Support arbitrary mesh/texture combinations per instance
- Fix cross-frame lighting buffer synchronization with dynamic offsets
- Add new pipelines (Skybox & Transparency) with separate render passes

[![VKEngine - Devlog 5](https://img.youtube.com/vi/82CNc7eAjmw/0.jpg)](https://www.youtube.com/watch?v=82CNc7eAjmw)


### Devlog 6
- Submeshes, Material Batching, Animations & Light Casters

[![VKEngine - Devlog 6](https://img.youtube.com/vi/sx5lNJ4Cczo/0.jpg)](https://www.youtube.com/watch?v=sx5lNJ4Cczo)

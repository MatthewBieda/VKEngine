<h1 align="center">VKEngine</h1>
<h3 align="center">A 3D game engine created with Vulkan and C++20.</h3>

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
  Update an object's model matrix in real-time to translate, rotate and scale any object. 

- **Light Casters**  
  A small cube mesh that tracks the positions of a point light, making placing lights easy.

- **Bindless Textures**  
  Uses descriptor indexing to give shaders direct access to large arrays of textures, eliminating the need to bind textures individually per draw call.

- **Frustum Culling**  
  Cull all objects that are not currently within the camera frustum in order to massively improve rendering efficiency. 

- **3D Audio**  
  Easy to use 2D and 3D Audio API provided by the SoLoud library.

- **AABB Debug Visualizations**  
  View Mesh-level and Submesh-level bounding boxes in real-time directly in the engine. 

### Roadmap:	

**Near-Term Goals**
- Normal Maps
- Skeletal Animations
- Cascaded Shadow Maps
- Deferred Rendering (Tiled Deferred + Forward Pass)
- Object Picking, ImGuizmo integration, scene serialization, CPU metrics (Editor features)

**Future / Experimental**
- PBR/IBL, Clustered Shading, Compute Shader post-processing, GPU-Driven Rendering, Mesh Shaders, Hardware Ray Tracing

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

### Devlog 7
- Frustum Culling, AABB debug visualizations, 3D Audio, Renderer Optimizations

[![VKEngine - Devlog 7](https://img.youtube.com/vi/8aeu3GFEH0M/0.jpg)](https://www.youtube.com/watch?v=8aeu3GFEH0M)

**Implementation Details**

- **CPU Culling**: The `performFrustumCulling` function iterates over all objects and performs visibility tests using their AABBs against the camera frustum. If an object passes the test, its global index is added to the `globalVisibleInstances` vector that will be pushed to a GPU buffer.

- **BuildDrawCommands**: This function takes that sparse list of visible object indices and groups them by mesh to enable instanced rendering. A nice optimization I added is Index Buffer Merging. If two sequential submeshes share the same material and belong to the same parent mesh, I don't create a new draw call. Instead, I just extend the `indexCount` of the previous draw command, effectively drawing both submeshes in a single call. This further reduces the total number of draw calls sent to the GPU.

- **Indirect GPU Rendering**: The GPU receives the grouped draw commands, but it needs to know which specific instance data to use. In the vertex shader, we use the built-in `gl_InstanceIndex`, which is sequential (0, 1, 2...). We use this sequential index to look up the global object index from our `visibleIndexData` buffer - this is the indirection step. Finally, we use that global index to fetch the correct Model Matrix and other per-object data from the main `ObjectBuffer`. This whole flow is an essential step towards GPU-Driven Rendering because the CPU's job is reduced to just filtering visibility and building draw commands - the GPU handles all the data fetching itself. In a fully GPU-driven pipeline, even the culling step would move to the GPU.

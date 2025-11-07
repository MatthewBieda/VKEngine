@echo off

cd /d "%~dp0"

glslc shader.vert -o vert.spv
glslc shader.frag -o frag.spv

glslc skybox.vert -o skyboxvert.spv
glslc skybox.frag -o skyboxfrag.spv

glslc debug.vert -o debug_vert.spv
glslc debug.frag -o debug_frag.spv

glslc shadow.vert -o shadow_vert.spv
glslc shadow.frag -o shadow_frag.spv

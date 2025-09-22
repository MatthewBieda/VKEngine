@echo off

cd /d "%~dp0"

glslc shader.vert -o vert.spv
glslc shader.frag -o frag.spv

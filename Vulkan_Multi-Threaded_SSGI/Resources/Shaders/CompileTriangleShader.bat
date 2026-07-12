@echo off
set GLSLANG=C:\VulkanSDK\1.3.261.1\Bin\glslangValidator.exe

%GLSLANG% -V triangle.vert -o triangle.vert.spv
%GLSLANG% -V triangle.frag -o triangle.frag.spv

echo Shaders compiled.
pause

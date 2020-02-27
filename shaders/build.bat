@echo off

set VULKAN_SHADER_COMPILER=C:/VulkanSDK/1.1.108.0/Bin32/glslangValidator.exe

If "%1" == "compile" goto compile
If "%1" == "all" goto all

:all

%VULKAN_SHADER_COMPILER% -V -o SPV/final.frag.spv final.frag
%VULKAN_SHADER_COMPILER% -V -o SPV/final.vert.spv final.vert

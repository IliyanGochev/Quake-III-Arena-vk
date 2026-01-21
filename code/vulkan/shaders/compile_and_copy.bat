@echo off
echo Compiling FSQ shaders...
glslangValidator -V fsq.vert -o compiled/fsq_vs.spv
glslangValidator -V fsq.frag -o compiled/fsq_ps.spv

echo Copying to baseq3...
copy /Y compiled\fsq_vs.spv E:\repos\Quake-III-Arena-vk\baseq3\vulkan\shaders\compiled\fsq_vs.spv
copy /Y compiled\fsq_ps.spv E:\repos\Quake-III-Arena-vk\baseq3\vulkan\shaders\compiled\fsq_ps.spv

echo Done!

# CHIP-8-Emulator
Emulator for CHIP-8 written in C++ with SDL-2.30.2

Platform layer ('platform.cpp' and 'platform.h') adapted from [https://austinmorlan.com/posts/chip8_emulator/] used for SDL2 integration.
Emulation logic (CPU, memory, instructions, programs) implemented entirely by me.

The purpose of this project was to introduce me to emulation and give me enough knowledge to do other emulation-related projects.

Compilation: Open the project in CLion, ensure SDL2 (version 2.30.2) is installed, and build CMakeLists.txt with main.cpp and platform files.

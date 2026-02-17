cmake_minimum_required(VERSION 3.20)
project(Chip8_Arduino)

set(CMAKE_CXX_STANDARD 20)

# 1. Add SDL2 as a subproject
add_subdirectory(3rdParty/sdl-2.30.2 EXCLUDE_FROM_ALL)

# 2. Define the executable
add_executable(Chip8
        main.cpp
        platform.cpp
)

# 3. Header search paths (.h)
target_include_directories(Chip8 PRIVATE
        3rdParty/sdl-2.30.2/include
        .
)

# 4. Link the SDL2 library
# We use the static version to make the .exe more portable.
target_link_libraries(Chip8 PRIVATE SDL2-static)
# Forces MinGW to include the C++ and the system libraries inside the .exe
target_link_options(Chip8 PRIVATE -static-libgcc -static-libstdc++ -static)
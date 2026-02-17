#pragma once

#include <cstdint>
#include "SDL.h"

class Platform
{
public:
    Platform(char const* title, int windowWidth, int windowHeight, int textureWidth, int textureHeight);
    ~Platform();
    void Update(void const* buffer, int pitch);
    bool ProcessInput(uint8_t* keys);

    void PlaySound(bool active);

private:
    SDL_Window* window{};
    SDL_Renderer* renderer{};
    SDL_Texture* texture{};

    SDL_AudioDeviceID audioDevice;
};
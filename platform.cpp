#include "platform.h"
#include "SDL.h"
#include <cmath>
#include <cstdint>


void AudioCallback(void* userdata, Uint8* stream, int len) {
    int16_t* buffer = (int16_t*)stream;
    int length = len / 2;

    static int32_t sampleIndex = 0;
    const int frequency = 440;
    const int sampleRate = 44100;
    const int amplitude = 3000;

    const int period = sampleRate / frequency;
    const int halfPeriod = period / 2;

    for (int i = 0; i < length; i++) {
        buffer[i] = ((sampleIndex / halfPeriod) % 2) ? (int16_t)amplitude : (int16_t)-amplitude;

        sampleIndex++;
        if (sampleIndex >= period) {
            sampleIndex = 0;
        }
    }
}

Platform::Platform(char const* title, int windowWidth, int windowHeight, int textureWidth, int textureHeight) {
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO);

    window = SDL_CreateWindow(
        title,
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        windowWidth, windowHeight,
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);

    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);

    texture = SDL_CreateTexture(
        renderer,
        SDL_PIXELFORMAT_RGBA8888,
        SDL_TEXTUREACCESS_STREAMING,
        textureWidth, textureHeight);

    SDL_AudioSpec want, have;
    SDL_zero(want);
    want.freq = 44100;
    want.format = AUDIO_S16SYS;
    want.channels = 1;
    want.samples = 2048;
    want.callback = AudioCallback;

    audioDevice = SDL_OpenAudioDevice(nullptr, 0, &want, &have, 0);

    SDL_PauseAudioDevice(audioDevice, 1);
}

Platform::~Platform() {
    SDL_CloseAudioDevice(audioDevice);
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
}

void Platform::PlaySound(bool active) {
    if (active) {
        SDL_PauseAudioDevice(audioDevice, 0);
    } else {
        SDL_PauseAudioDevice(audioDevice, 1);
    }
}

void Platform::Update(void const* buffer, int pitch) {
    SDL_UpdateTexture(texture, nullptr, buffer, pitch);
    SDL_RenderClear(renderer);
    SDL_RenderCopy(renderer, texture, nullptr, nullptr);
    SDL_RenderPresent(renderer);
}

bool Platform::ProcessInput(uint8_t* keys) {
    bool quit = false;
    SDL_Event event;

    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) quit = true;

        if (event.type == SDL_KEYDOWN) {
            if (event.key.keysym.sym == SDLK_ESCAPE) quit = true;

            switch (event.key.keysym.sym) {
                case SDLK_x: keys[0] = 1; break;
                case SDLK_1: keys[1] = 1; break;
                case SDLK_2: keys[2] = 1; break;
                case SDLK_3: keys[3] = 1; break;
                case SDLK_q: keys[4] = 1; break;
                case SDLK_w: keys[5] = 1; break;
                case SDLK_e: keys[6] = 1; break;
                case SDLK_a: keys[7] = 1; break;
                case SDLK_s: keys[8] = 1; break;
                case SDLK_d: keys[9] = 1; break;
                case SDLK_z: keys[0xA] = 1; break;
                case SDLK_c: keys[0xB] = 1; break;
                case SDLK_4: keys[0xC] = 1; break;
                case SDLK_r: keys[0xD] = 1; break;
                case SDLK_f: keys[0xE] = 1; break;
                case SDLK_v: keys[0xF] = 1; break;
            }
        }

        if (event.type == SDL_KEYUP) {
            switch (event.key.keysym.sym) {
                case SDLK_x: keys[0] = 0; break;
                case SDLK_1: keys[1] = 0; break;
                case SDLK_2: keys[2] = 0; break;
                case SDLK_3: keys[3] = 0; break;
                case SDLK_q: keys[4] = 0; break;
                case SDLK_w: keys[5] = 0; break;
                case SDLK_e: keys[6] = 0; break;
                case SDLK_a: keys[7] = 0; break;
                case SDLK_s: keys[8] = 0; break;
                case SDLK_d: keys[9] = 0; break;
                case SDLK_z: keys[0xA] = 0; break;
                case SDLK_c: keys[0xB] = 0; break;
                case SDLK_4: keys[0xC] = 0; break;
                case SDLK_r: keys[0xD] = 0; break;
                case SDLK_f: keys[0xE] = 0; break;
                case SDLK_v: keys[0xF] = 0; break;
            }
        }
    }
    return quit;
}
#include <chrono>
#include <cstring>
#include <fstream>
#include <random>
#include <cstdio>

#define SDL_MAIN_HANDLED
#include "platform.h"

// -- Constants --
const unsigned int FONTSET_SIZE = 80;
const unsigned int FONTSET_START_ADDRESS = 0x50;
const unsigned int START_ADDRESS = 0x200;
const unsigned int KEY_COUNT = 16;
const unsigned int MEMORY_SIZE = 4096;
const unsigned int REGISTER_COUNT = 16;
const unsigned int STACK_LEVELS = 16;
const unsigned int VIDEO_HEIGHT = 32;
const unsigned int VIDEO_WIDTH = 64;

// -- Global System Variables --
uint8_t keypad[KEY_COUNT]{};
uint32_t video[VIDEO_HEIGHT * VIDEO_WIDTH]{}; //32-bit buffer for SLD2

uint8_t memory[MEMORY_SIZE]{};
uint8_t registers[REGISTER_COUNT]{};
uint16_t index_reg{};
uint16_t pc{};
uint8_t delayTimer{};
uint8_t soundTimer{};
uint16_t stack[STACK_LEVELS]{};
uint8_t sp{};
uint16_t opcode{};

std::default_random_engine randGen;
std::uniform_int_distribution<uint8_t> randByte;

// -- CPU Operations --
void rippleCarry(uint8_t* A, int B, bool Cin, bool Carry){
    int result = 0;
    bool sub = Cin;
    B = sub ? ~B : B;
    //Add
    for(int i = 0; i < 8; ++i)
    {
        //Extract the current bit
        bool bitA = (*A >> i) & 0x1;
        bool bitB = (B >> i) & 0x1;
        //Result of the full adder later added to the result
        bool sum = false;
        //--Full Adder--
        bool firstXOR = bitA ^ bitB;
        sum = firstXOR ^ Cin;
        //Gets the Cout for the next Full Adder
        bool firstAND = bitA & bitB;
        bool secondAND = firstXOR & Cin;
        Cin = firstAND | secondAND;
        //Gets result into the 8-bit integer
        if (sum)    result |= sum << i;
    }
	if (Carry) registers[0xF] = Cin ? 1 : 0;
    *A = result;
}

// -- Fontset --
uint8_t fontset[FONTSET_SIZE] =
{
	0xF0, 0x90, 0x90, 0x90, 0xF0, // 0
	0x20, 0x60, 0x20, 0x20, 0x70, // 1
	0xF0, 0x10, 0xF0, 0x80, 0xF0, // 2
	0xF0, 0x10, 0xF0, 0x10, 0xF0, // 3
	0x90, 0x90, 0xF0, 0x10, 0x10, // 4
	0xF0, 0x80, 0xF0, 0x10, 0xF0, // 5
	0xF0, 0x80, 0xF0, 0x90, 0xF0, // 6
	0xF0, 0x10, 0x20, 0x40, 0x40, // 7
	0xF0, 0x90, 0xF0, 0x90, 0xF0, // 8
	0xF0, 0x90, 0xF0, 0x10, 0xF0, // 9
	0xF0, 0x90, 0xF0, 0x90, 0x90, // A
	0xE0, 0x90, 0xE0, 0x90, 0xE0, // B
	0xF0, 0x80, 0x80, 0x80, 0xF0, // C
	0xE0, 0x90, 0x90, 0x90, 0xE0, // D
	0xF0, 0x80, 0xF0, 0x80, 0xF0, // E
	0xF0, 0x80, 0xF0, 0x80, 0x80  // F
};

uint8_t ROM[]{
	0x60, 0x00, // LD V0, 0x00 - Counter
	0x61, 0x06, // LD V1, 0x06 - X Coordinate
	0x62, 0x06, // LD V2, 0x06 - Y Coordinate
	0x63, 0x05, // LD V3, 0x05 - Memory jump
	0xA2, 0x18, // LD I, 0x218 - Fontset address
	0xD1, 0x25, // DRW V1, V2, 0x5 - 0x20A, draw
	0x71, 0x06, // ADD V1, 0x06 - Move X coordinate
	0xF3, 0x1E, // ADD I, V3 - Next sprite
	0x70, 0x01, // ADD V0, 0x01 - Adds 1 to the counter
	0x30, 0x06, // SE V0, 0x06 (if V0 == 6) break; - Skips next instruction
	0x12, 0x0A, // JMP 0x20A - Jumps to draw
	0xF1, 0x0A, // LD VX, K - Pause execution
	// Sprite data
	0x90, // 1001 0000
	0x90, // 1001 0000
	0xF0, // 1111 0000
	0x90, // 1001 0000
	0x90, // 1001 0000

	0xF0, // 1111 0000
	0x80, // 1000 0000
	0xF0, // 1111 0000
	0x80, // 1000 0000
	0xF0, // 1111 0000

	0x80, // 1000 0000
	0x80, // 1000 0000
	0x80, // 1000 0000
	0x80, // 1000 0000
	0xF0, // 1111 0000

	0x80, // 1000 0000
	0x80, // 1000 0000
	0x80, // 1000 0000
	0x80, // 1000 0000
	0xF0, // 1111 0000

	0xF0, // 1111 0000
	0x90, // 1001 0000
	0x90, // 1001 0000
	0x90, // 1001 0000
	0xF0, // 1111 0000

	0xA0, // 1010 0000
	0xA0, // 1010 0000
	0xA0, // 1010 0000
	0x00, // 0000 0000
	0xA0, // 1010 0000
};

// -- Initialization --
void InitCHIP8() {
    pc = START_ADDRESS;
    for (unsigned int i = 0; i < FONTSET_SIZE; ++i) {
        memory[FONTSET_START_ADDRESS + i] = fontset[i];
    }
    randGen.seed(std::chrono::system_clock::now().time_since_epoch().count());
    randByte = std::uniform_int_distribution<uint8_t>(0, 255);
}

void LoadROM() {
    for (uint16_t i = 0; i < sizeof(ROM); ++i) memory[START_ADDRESS + i] = ROM[i];
}

// -- CPU instructions --

void OP_NULL()
{}

void OP_Dxyn(uint8_t Vx, uint8_t Vy, uint8_t height)
{
	uint8_t xPos = registers[Vx] & (VIDEO_WIDTH - 1);
	uint8_t yPos = registers[Vy] & (VIDEO_HEIGHT - 1);
	registers[0xF] = 0;
	for (unsigned int row = 0; row < height; ++row)
	{
		uint8_t spriteByte = memory[index_reg + row];
		for (unsigned int col = 0; col < 8; ++col)
		{
			uint8_t spritePixel = spriteByte & (0x80u >> col);
			uint32_t* screenPixel = &video[(yPos + row) * VIDEO_WIDTH + (xPos + col)];
			if (spritePixel)
			{
				if (*screenPixel == 0xFFFFFFFF)
				{
					registers[0xF] = 1;
				}
				*screenPixel ^= 0xFFFFFFFF;
			}
		}
	}
}

void OP_Fx0A(uint8_t Vx)
{
    bool keyPressed = false;
    for (uint8_t i = 0; i < KEY_COUNT; ++i)
    {
        if (keypad[i])
        {
            registers[Vx] = i;
            keyPressed = true;
            break;
        }
    }
    if (!keyPressed)
    {
        pc -= 2;
    }
}

void OP_Fx33(uint8_t Vx)
{
	uint8_t value = registers[Vx];
	memory[index_reg + 2] = value % 10;
	value /= 10;
	memory[index_reg + 1] = value % 10;
	value /= 10;
	memory[index_reg] = value % 10;
}

// -- Decoding Tables --
void Table0(uint8_t kk)
{
    switch (kk) {
		case 0xE0u: memset(video, 0, sizeof(video)); break;
		case 0xEEu: pc = stack[--sp]; break;
		default: OP_NULL(); break;
	}
}

void Table8(uint8_t x, uint8_t y, uint8_t n) {
    switch (n) {
		case 0x0u: registers[x] = registers[y]; break;
		case 0x1u: registers[x] |= registers[y]; break;
		case 0x2u: registers[x] &= registers[y]; break;
		case 0x3u: registers[x] ^= registers[y]; break;

		case 0x4u: rippleCarry(&registers[x], registers[y], false, true); break;
		case 0x5u: rippleCarry(&registers[x], registers[y], true, true); break;

		case 0x6u:
			registers[0xFu] = registers[x] & 0x1u;
			registers[x] >>= 1;
			break;

		case 0xEu:
			registers[0xFu] = (registers[x] & 0x80u) >> 7;
			registers[x] <<= 1;
			break;

		default: OP_NULL();
	}
}

void TableE(uint8_t x, uint8_t kk) {
	switch (kk) {
		case 0x9Eu: if (keypad[registers[x]]) pc += 2; break;
		case 0xA1u: if (!keypad[registers[x]]) pc += 2; break;
		default: OP_NULL(); break;
    }
}

void TableF(uint8_t x, uint8_t kk) {
    switch (kk) {
        case 0x07u: registers[x] = delayTimer; break;
        case 0x0Au: OP_Fx0A(x); break;
        case 0x15u: delayTimer = registers[x]; break;
        case 0x18u: soundTimer = registers[x]; break;
        case 0x1Eu: index_reg += registers[x]; break;
        case 0x29u: index_reg = FONTSET_START_ADDRESS + (5 * registers[x]); break;
        case 0x33u: OP_Fx33(x); break;
        case 0x55u: for (uint8_t i = 0; i <= x; ++i) memory[index_reg + i] = registers[i]; break;
        case 0x65u: for (uint8_t i = 0; i <= x; ++i) registers[i] = memory[index_reg + i]; break;
        default: OP_NULL(); break;
    }
}

// -- Cycle --
void Cycle() {
    opcode = (memory[pc] << 8u) | memory[pc + 1];
    pc += 2;

    uint8_t x = (opcode & 0x0F00u) >> 8u;
    uint8_t y = (opcode & 0x00F0u) >> 4u;
    uint8_t n =  opcode & 0x000Fu;
    uint8_t kk = opcode & 0x00FFu;
    uint16_t nnn = opcode & 0x0FFFu;

    switch (opcode & 0xF000u){
		case 0x0000u: Table0(kk); break;
		case 0x1000u: pc = nnn; break;
		case 0x2000u: stack[sp++] = pc; pc = nnn; break;
		case 0x3000u: if (registers[x] == kk) pc += 2; break;
		case 0x4000u: if (registers[x] != kk)	pc += 2; break;
        case 0x5000u: if (registers[x] == registers[y]) pc += 2; break;
		case 0x6000u: registers[x] = kk; break;
		case 0x7000u: rippleCarry(&registers[x], kk, false, false); break;
		case 0x8000u: Table8(x, y, n); break;
        case 0x9000u: if (registers[x] != registers[y]) pc += 2; break;
		case 0xA000u: index_reg = nnn; break;
        case 0xB000u: pc = registers[0] + nnn; break;
		case 0xC000u: registers[x] = randByte(randGen) & kk; break;
		case 0xD000U: OP_Dxyn(x, y, n); break;
		case 0xE000u: TableE(x, kk); break;
		case 0xF000u: TableF(x, kk); break;
		default: OP_NULL(); break;
	}
    if (delayTimer > 0) --delayTimer;
    if (soundTimer > 0) --soundTimer;
}

int main() {
	Platform platform("CHIP-8 Emulator", VIDEO_WIDTH * 10, VIDEO_HEIGHT * 10, VIDEO_WIDTH, VIDEO_HEIGHT);

	InitCHIP8();
	LoadROM();

	int videoPitch = sizeof(video[0]) * VIDEO_WIDTH;
	auto lastCycleTime = std::chrono::high_resolution_clock::now();
	auto lastTimerTime = std::chrono::high_resolution_clock::now();
	bool quit = false;

	while (!quit) {
		quit = platform.ProcessInput(keypad);

		auto currentTime = std::chrono::high_resolution_clock::now();

		float dt_cpu = std::chrono::duration<float, std::chrono::milliseconds::period>(currentTime - lastCycleTime).count();
		if (dt_cpu > 2) {
			lastCycleTime = currentTime;
			Cycle();
			platform.Update(video, videoPitch);
		}

		float dt_timers = std::chrono::duration<float, std::chrono::milliseconds::period>(currentTime - lastTimerTime).count();
		if (dt_timers > 16.66f) {
			lastTimerTime = currentTime;

			if (delayTimer > 0) --delayTimer;

			if (soundTimer > 0) {
				platform.PlaySound(true);
				--soundTimer;
			} else {
				platform.PlaySound(false);
			}
		}
	}
	return 0;
}
#include <chrono>
#include <cstring>
#include <random>

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
    // Add
    for(int i = 0; i < 8; ++i)
    {
        // Extract the current bit
        bool bitA = (*A >> i) & 0x1;
        bool bitB = (B >> i) & 0x1;
        // Result of the full adder later added to the result
        bool sum = false;
        // -- Full Adder --
        bool firstXOR = bitA ^ bitB;
        sum = firstXOR ^ Cin;
        // Gets the Cout for the next Full Adder
        bool firstAND = bitA & bitB;
        bool secondAND = firstXOR & Cin;
        Cin = firstAND | secondAND;
        // Gets result into the 8-bit integer
        if (sum)    result |= sum << i;
    }
	*A = result;
	if (Carry) registers[0xF] = Cin ? 1 : 0;
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

// -- Programmable ROM --

uint8_t ROM[] {
	// -- SNAKE --
	/*
	 * USE WASD TO MOVE
	 */
	/* REGISTERS:
	 * V0 = Player's direction
	 * V1 = Player's X Coordinate
	 * V2 = Player's Y Coordinate
	 * V3 = Tail's direction
	 * V4 = Tail's X Coordinate
	 * V5 = Tail's Y Coordinate
	 * V6 = No use
	 * V7 = Apple's X Coordinate
	 * V8 = Apple's Y Coordinate
	 * V9 = Head's array index
	 * VA = Tail's array index
	 * VB = Temporary direction
	 * VC = Temporary X Coordinate
	 * VD = Temporary Y Coordinate
	 * VE = Apple/Sound flag
	 * VF = Collision Flag
	 */
	/*
	 * 0x37A = DATA STRUCT
	 */
	0x12, 0x38, // 0x200	JP 0x238
	// -- SPRITES --
	0x80, 0x00, // 0x202	1000 0000 Apple/Snake sprite
	0xFF, 0x00, // 0x204	1111 1111 0000 0000 Roof/Floor sprite
	0x80, 0x80, // 0x206	1000 0000 1000 0000 Wall
	// -- DRAW HORIZONTAL--
	0xA2, 0x04, // 0x208	LD I, 0x204 - Roof Sprite
	0xD0, 0x11, // 0x20A	DRW V0, V1, 1 - Draw 8 horizontal pixels
	0x70, 0x08, // 0x20C	ADD V0, 0x08 - Add 8 to X Coordinate
	0x30, 0x40, // 0x20E	SE V0, 0x40 - Ends cycle if X Coordinate == 64
	0x12, 0x0A, // 0x210	JP 0x20A - Restarts the cycle
	0x00, 0xEE, // 0x212    RET - return
	// -- DRAW VERTICAL --
	0xA2, 0x06, // 0x214	LD I, 0x206 - Wall Sprite
	0xD0, 0x12, // 0x216	DRW V0, V1, 2 - Draw 2 vertical pixels
	0x71, 0x02, // 0x218	ADD V1, 0x02 - Add 2 to the Y Coordinate
	0x31, 0x1F, // 0x21A	SE V1, 0x1E - Ends cycle if Y Coordinate == 30
	0x12, 0x16, // 0x21C	JP 0x216 - Restarts the cycle
	0x00, 0xEE, // 0x21E    RET - return
	// -- START PLAYER --
	0xA2, 0x02, // 0x220	LD I, 0x202 - Apple Sprite
	0xD1, 0x21, // 0x222	DRW V1, V2, 1 - Draw 1 pixel
	0x71, 0xFF, // 0x224	ADD V1, 0xFF - ADD 255 (-1) to X Coordinate
	0xD1, 0x21, // 0x226	DRW V1, V2, 1 - Draw 1 pixel
	0x00, 0xEE, // 0x228    RET - return
	// -- DRAW APPLE --
	0xC7, 0x3F, // 0x22A	RND V7, 0x3F - Random number from 0 to 63
	0xC8, 0x1F, // 0x22C	RND V8, 0x1F - Random number from 0 to 31
	0xD7, 0x81, // 0x22E	DRW V7, V8, 1 - Draw 1 pixel
	0x3F, 0x01, // 0x230	SE VF, 0x01 - Checks if VF flag is set to 1 (collision)
		// -- DRAW MISSING PIXEL (if-else result) --
	0x00, 0xEE, // 0x232    RET - return
	0xD7, 0x81, // 0x234	DRW V7, V8, 1 - else draws missing pixel
	0x12, 0x2A, // 0x236	JP 0x22A - Jumps to redraw apple

	// -- SCREEN SETUP --
		// -- DRAW ROOF --
	0x60, 0x00, // 0x238    LD V0, 0x00 - X Coordinate
	0x61, 0x00, // 0x23A    LD V1, 0x00 - Y Coordinate
	0x22, 0x08, // 0x23C	CALL 0x208 - Calls DRAW HORIZONTAL
		// -- DRAW FLOOR --
	0x60, 0x00, // 0x23E    LD V0, 0x00 - X Coordinate
	0x61, 0x1F, // 0x240    LD V1, 0x1F - Y Coordinate
	0x22, 0x08, // 0x242	CALL 0x208 - Calls DRAW HORIZONTAL
		// -- DRAW LEFT WALL --
	0x60, 0x00, // 0x244    LD V0, 0x00 - X Coordinate
	0x61, 0x01, // 0x246    LD V1, 0x01 - Y Coordinate
	0x22, 0x14, // 0x248	CALL 0x214 - Calls DRAW VERTICAL
		// -- DRAW RIGHT WALL --
	0x60, 0x3F, // 0x24A    LD V0, 0x3F - X Coordinate
	0x61, 0x01, // 0x24C    LD V1, 0x01 - Y Coordinate
	0x22, 0x14, // 0x24E	CALL 0x214 - Calls DRAW VERTICAL
		// -- DRAW PLAYER --
	0x61, 0x1F,	// 0x250	LD V1, 0x1F - X Coordinate
	0x62, 0x0F, // 0x252    LD V2, 0x0F - Y Coordinate
	0x22, 0x20, // 0x254	CALL 0x220 - Calls START PLAYER
	0x71, 0x01, // 0x256    ADD V1, 0x01 - Player's head gets corrected
		// -- DRAW APPLE --
	0x22, 0x2A, // 0x258	CALL 0x22A - Calls DRAW APPLE

	// -- SETTING UP VARIABLES --
		// -- MOTION VECTOR --
	0x60, 0x09, // 0x25A	LD V0, 0x09
		// -- DELAY TIMER --
	0x66, 0x00, // 0x25C	LD V6, 0x00
		// -- TAIL'S COORDINATES --
	0x63, 0x09, // 0x25E    LD V3, 0x09 - Direction
	0x64, 0x1D, // 0x260    LD V4, 0x1D - X Coordinate
	0x65, 0x0F, // 0x262    LD V5, 0x0F - Y Coordinate

	// -- JUMP TO MAIN --
	0x13, 0x64, // 0x264	JP 0x364 - Jumps to Main Loop

	// -- GAME FUNCTIONS--
		// -- MOVEMENT (Switch Case) --
	0x50, 0xB0, // 0x266	SE V0, VB - Verifies if new direction is different from the old one
	0x22, 0xE4, // 0x268	CALL 0x2E4 - Stores data into struct
	0x80, 0x0E, // 0x26A    SHL V0 {, V0} - Multiplies by 2 to avoid odd memory addresses
	0xB2, 0x6E,	// 0x26C	JP V0, 0x26A - Reads Pressed Key
	0x00, 0xEE, // 0x26E	RET - CASE: 0
	0x00, 0xEE, // 0x270	RET - CASE: 1
	0x00, 0xEE, // 0x272	RET - CASE: 2
	0x00, 0xEE, // 0x274	RET - CASE: 3
	0x00, 0xEE, // 0x276	RET - CASE: 4
	0x12, 0x8E, // 0x278	JP 0x28E - CASE: 5 - W
	0x00, 0xEE, // 0x27A	RET - CASE: 6
	0x12, 0xAC, // 0x27C	JP 0x2AC - CASE: 7 - A
	0x12, 0xA2, // 0x27E	JP 0x2A2 - CASE: 8 - S
	0x12, 0x98, // 0x280	JP 0x298- CASE: 9 - D
	0x00, 0xEE, // 0x282	RET - CASE: A
	0x00, 0xEE, // 0x284	RET - CASE: B
	0x00, 0xEE, // 0x286	RET - CASE: C
	0x00, 0xEE, // 0x288	RET - CASE: D
	0x00, 0xEE, // 0x28A	RET - CASE: E
	0x00, 0xEE, // 0x28C	RET - CASE: F

		// -- UP MOVEMENT  --
	0xA2, 0x02, // 0x28E	LD I, 0x202 - Apple Sprite
	0x72, 0xFF, // 0x290	ADD V2, 0xFF - SUB 1 to Y Coordinate
	0xD1, 0x21, // 0x292	DRW V1, V2, 1
	0x00, 0x00, // 0x294	No operation
	0x00, 0xEE, // 0x296    RET - return

		// -- RIGHT MOVEMENT --
	0xA2, 0x02, // 0x298	LD I, 0x202 - Apple Sprite
	0x71, 0x01, // 0x29A	ADD V1, 0x01 - ADD 1 to X Coordinate
	0xD1, 0x21, // 0x29C	DRW V1, V2, 1
	0x00, 0x00, // 0x29E	No operation
	0x00, 0xEE, // 0x2A0    RET - return

		// -- DOWN MOVEMENT --
	0xA2, 0x02, // 0x2A2	LD I, 0x202 - Apple Sprite
	0x72, 0x01, // 0x2A4	ADD V2, 0x01 - ADD 1 to Y Coordinate
	0xD1, 0x21, // 0x2A6	DRW V1, V2, 1
	0x00, 0x00, // 0x2A8	No operation
	0x00, 0xEE, // 0x2AA    RET - return

		// -- LEFT MOVEMENT --
	0xA2, 0x02, // 0x2AC	LD I, 0x202 - Apple Sprite
	0x71, 0xFF, // 0x2AE	ADD V1, 0xFF - SUB 1 to X Coordinate
	0xD1, 0x21, // 0x2B0	DRW V1, V2, 1
	0x00, 0x00, // 0x2B2	No operation
	0x00, 0xEE, // 0x2B4    RET - return

		// -- CHECK COLLISION --
	0x91, 0x70, // 0x2B6	SNE V1, V7 - Jumps to Game Over if V1 != V7
	0x52, 0x80, // 0x2B8	SE V2, V8 - Jumps to Game Over if V2 != V8
	0x13, 0x60, // 0x2BA	JP 0x360 - Jumps to GAME OVER
	0xD1, 0x21, // 0x2BC	DRW V1, V2, 1 - Player's head gets redraw
	0x22, 0x2A, // 0x2BE	CALL 0x22A - Calls DRAW APPLE
	0x6E, 0x05, // 0x2C0	LD VE, 0x05
	0xFE, 0x18, // 0x2C2	LD ST, VE - Loads Sound Timer with V3
	0x00, 0xEE, // 0x2C4	RET - return

		// -- READ KEYS (HEAD'S MOVEMENT) --
	0x8B, 0x00, // 0x2C6	LD VB, V0 - Saves current direction in a temporary register
	0x60, 0x05, // 0x2C8	LD V0, 0x5 - Loads in V0 the value of W
	0xE0, 0xA1, // 0x2CA	SKNP V0 - If W is pressed jumps into Switch Case
	0x12, 0x66, // 0x2CC	JP 0x266 - Jumps to Switch Case
	0x60, 0x07, // 0x2CE	LD V0, 0x7 - Loads in V0 the value of A
	0xE0, 0xA1, // 0x2D0	SKNP V0 - If A is pressed jumps into Switch Case
	0x12, 0x66, // 0x2D2	JP 0x266 - Jumps to Switch Case
	0x60, 0x08, // 0x2D4	LD V0, 0x8 - Loads in V0 the value of S
	0xE0, 0xA1, // 0x2D6	SKNP V0 - If S is pressed jumps into Switch Case
	0x12, 0x66, // 0x2D8	JP 0x266 - Jumps to Switch Case
	0x60, 0x09, // 0x2DA	LD V0, 0x9 - Loads in V0 the value of D
	0xE0, 0xA1, // 0x2DC	SKNP V0 - If D is pressed jumps into Switch Case
	0x12, 0x66, // 0x2DE	JP 0x266 - Jumps to Switch Case
	0x80, 0xB0, // 0x2E0	LD V0, VB - If none of the keys above gets pressed, reloads previous direction
	0x12, 0x66, // 0x2E2	JP 0x266 - Jumps to Switch Case

		// -- STORE HEAD IN STRUCT --
	0xA3, 0x80, // 0x2E4	LD I 0x380 - Selects the struct
	0x49, 0x40, // 0x2E6	SNE V9, 0x40
	0x69, 0x00, // 0x2E8	LD V9, 0x00 - Sets index to 0 if V9 == 64
	0x89, 0x9E, // 0x2EA	SHL V9 {, V9} - Multiplies by 4 the head's index
	0x89, 0x9E, // 0x2EC	SHL V9 {, V9} - Power-of-Two Alignment
	0xF9, 0x1E, // 0x2EE	ADD I, V9 - Selects the array index
	0xF3, 0x55, // 0x2F0	LD I, V3 - Stores from register 0 to 3 in memory
	0x89, 0x96, // 0x2F2	SHR V9 {, V9} - Returns index back to normal
	0x89, 0x96, // 0x2F4	SHR V9 {, V9}
	0x79, 0x01, // 0x2F6	ADD V9, 0x01 - Adds 1 to the index
	0x00, 0xEE, // 0x2F8	RET - return

		// -- STORE HEAD IN TEMPORARY REGISTERS --
	0x8B, 0x00, // 0x2FA	LD VB, V0
	0x8C, 0x10, // 0x2FC	LD VC, V1
	0x8D, 0x20, // 0x2FE	LD VD, V2
	0x00, 0xEE, // 0x300	RET - return

		// -- RESTORE HEAD FROM TEMPORARY REGISTERS --
	0x80, 0xB0, // 0x302	LD V0, VB
	0x81, 0xC0, // 0x304	LD V1, VC
	0x82, 0xD0, // 0x306	LD V2, VD
	0x00, 0xEE, // 0x308	RET - return

		// -- STORE TAIL IN MAIN REGISTERS --
	0x80, 0x30, // 0x30A	LD V0, V3
	0x81, 0x40, // 0x30C	LD V1, V4
	0x82, 0x50, // 0x30E	LD V2, V5
	0x00, 0xEE, // 0x310	RET - return

		// -- RESTORE TAIL FROM MAIN REGISTERS --
	0x83, 0x00, // 0x312	LD V3, V0
	0x84, 0x10, // 0x314	LD V4, V1
	0x85, 0x20, // 0x316	LD V5, V2
	0x00, 0xEE, // 0x318	RET - return

		// -- READ STRUCT --
	0xA3, 0x80, // 0x31A	LD I 0x380 - Selects the struct
	0x4A, 0x40, // 0x31C	SNE VA, 0x40
	0x6A, 0x00, // 0x31E	LD VA, 0x00 - Sets index to 0 if VA == 64
	0x8A, 0xAE, // 0x320	SHL VA {, VA} - Multiplies by 4 the head's index
	0x8A, 0xAE, // 0x322	SHL VA {, VA} - Power-of-Two Alignment
	0xFA, 0x1E, // 0x324	ADD I, VA - Selects the array index
	0xF2, 0x65, // 0x326	LD I, V2 - Stores to register 0 to 2 from memory
	0x8A, 0xA6, // 0x328	SHR VA {, VA} - Returns index back to normal
	0x8A, 0xA6, // 0x32A	SHR VA {, VA}
	0x00, 0xEE, // 0x32C	RET - return

		// -- UPDATE TAIL --
	0x91, 0x40, // 0x32E	SNE V1, V4 - Jumps to RET if V1 != V4
	0x52, 0x50, // 0x330	SE V2, V5 - Jumps to RET if V2 != V5
	0x00, 0xEE, // 0x332	RET - return
	0x7A, 0x01, // 0x334	ADD VA, 0x01 - Increment Tail's index
	0x23, 0x12, // 0x336	CALL 0x312 - RESTORE TAIL FROM MAIN REGISTERS
	0x00, 0xEE, // 0x338	RET - return

		// -- MOVE HEAD'S DATA --
	0x22, 0xFA, // 0x33A	CALL 0x2FA - Calls STORE HEAD IN TEMPORARY REGISTERS
	0x23, 0x1A, // 0x33C	CALL 0x31A - Calls READ STRUCT
	0x23, 0x2E, // 0x33E	CALL 0x32E - Calls UPDATE TAIL
	0x23, 0x0A, // 0x340	CALL 0x30A - Calls STORE TAIL IN MAIN REGISTERS
	0x22, 0x6A, // 0x342	CALL 0x26A - Draws tail
	0x80, 0x06, // 0x344	SHR V0 {, V0} - Divides V0 by 2
	0x23, 0x12, // 0x346	CALL 0x312 - Calls RESTORE TAIL FROM MAIN REGISTERS
	0x23, 0x02, // 0x348	CALL 0x302 - Calls RESTORE HEAD FROM TEMPORARY REGISTERS
	0x00, 0xEE, // 0x34A	RET - return

		// -- MOVE TAIL'S DATA --
	0x22, 0xFA, // 0x34C	CALL 0x2FA - Calls STORE HEAD IN TEMPORARY REGISTERS
	0x23, 0x0A, // 0x34E	CALL 0x30A - Calls STORE TAIL IN MAIN REGISTERS
	0x22, 0x6A, // 0x350	CALL 0x26A - Draws tail
	0x80, 0x06, // 0x352	SHR V0 {, V0} - Divides V0 by 2
	0x23, 0x12, // 0x354	CALL 0x312 - Calls RESTORE TAIL FROM MAIN REGISTERS
	0x23, 0x02, // 0x356	CALL 0x302 - Calls RESTORE HEAD FROM TEMPORARY REGISTERS
	0x00, 0xEE, // 0x358	RET - return

		// -- UPDATE TAIL --
	0x59, 0xA0, // 0x35A	SE V9, VA - If V9 != VA calls MOVE HEAD'S DATA
	0x13, 0x3A, // 0x35C	JP 0x33A - Jumps to MOVE HEAD'S DATA
	0x13, 0x4C, // 0x35E	JP 0x34C - Jumps to MOVE TAIL'S DATA

		// -- GAME OVER --
	0xF0, 0x0A, // 0x360	LD V0, K - Stops execution until key pressed
	0x13, 0x60, // 0x362	JP 0x360 - Jumps to GAME OVER

	// -- MAIN LOOP --
	0x22, 0xC6, // 0x364	CALL 0x2C6 - Read input and draw head
	0x4F, 0x01, // 0x366	SNE VF, 0x01 - If VF == 1 verifies collision
	0x22, 0xB6, // 0x368	CALL 0x2B6 - Calls CHECK COLLISION
	0x80, 0x06, // 0x36A	SHR V0 {, V0} - Divides V0 by 2
	0x3E, 0x05, // 0x36C	SE VE, 0x01 - If VE != 5 Snake grows by 1
	0x23, 0x5A, // 0x36E	CALL 0x35A - Calls UPDATE TAIL
	0x6E, 0x00, // 0x370	LD VE, 0x01 - Loads VE with 0x00 (Resets Apple/Sound Flag)
	0x13, 0x64, // 0x372	JP 0x364 - Repeats cycle
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
	uint8_t xStart = registers[Vx] & (VIDEO_WIDTH - 1);
	uint8_t yStart = registers[Vy] & (VIDEO_HEIGHT - 1);
	registers[0xF] = 0;

	for (uint8_t row = 0; row < height; ++row)
	{
		uint8_t y = (yStart + row) & (VIDEO_HEIGHT - 1);
		uint8_t spriteByte = memory[index_reg + row];

		for (uint8_t col = 0; col < 8; ++col)
		{
			uint8_t x = (xStart + col) & (VIDEO_WIDTH - 1);

			if ((spriteByte & (0x80u >> col)) != 0)
			{
				uint32_t* screenPixel = &video[y * VIDEO_WIDTH + x];

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
        case 0xB000u: pc = (nnn + registers[0]) & 0xFFF; break;
		case 0xC000u: registers[x] = randByte(randGen) & kk; break;
		case 0xD000U: OP_Dxyn(x, y, n); break;
		case 0xE000u: TableE(x, kk); break;
		case 0xF000u: TableF(x, kk); break;
		default: OP_NULL(); break;
	}
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
		}

		float dt_timers = std::chrono::duration<float, std::chrono::milliseconds::period>(currentTime - lastTimerTime).count();
		if (dt_timers > 16.66f) {
			platform.Update(video, videoPitch);
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

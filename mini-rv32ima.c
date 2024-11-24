// Copyright 2022 Charles Lohr, you may use this file or any portions herein under any of the BSD, MIT, or CC0 licenses.

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "default64mbdtc.h"

// Just default RAM amount is 64MB.
uint32_t ram_amt = 64 * 1024 * 1024;
int fail_on_all_faults = 0;

static uint64_t GetTimeMicroseconds();
static uint32_t HandleException(uint32_t ir, uint32_t retval);
static uint32_t HandleControlStore(uint32_t addy, uint32_t val);
static uint32_t HandleControlLoad(uint32_t addy);
static void MiniSleep();
static int IsKBHit();
static int ReadKBByte();

// This is the functionality we want to override in the emulator.
//  think of this as the way the emulator's processor is connected to the outside world.
#define MINIRV32WARN(x...) printf(x);
#define MINIRV32_DECORATE static
#define MINI_RV32_RAM_SIZE ram_amt
#define MINIRV32_IMPLEMENTATION
#define MINIRV32_POSTEXEC(pc, ir, retval)             \
	{                                                 \
		if (retval > 0)                               \
		{                                             \
			if (fail_on_all_faults)                   \
			{                                         \
				printf("FAULT\n");                    \
				return 3;                             \
			}                                         \
			else                                      \
				retval = HandleException(ir, retval); \
		}                                             \
	}
#define MINIRV32_HANDLE_MEM_STORE_CONTROL(addy, val) \
	if (HandleControlStore(addy, val))               \
		return val;
#define MINIRV32_HANDLE_MEM_LOAD_CONTROL(addy, rval) rval = HandleControlLoad(addy);

#include "mini-rv32ima.h"

uint8_t *ram_image = 0;
struct MiniRV32IMAState *core;
const char *kernel_command_line = 0;
long long instct = -1;

void DumpState(struct MiniRV32IMAState *core, uint8_t *ram_image);
void LoadImage();

void PerformOneCycle();
void Initialize();

char outputBuffer[10240];
int outputBufferCount = 0;

char inputBuffer[512];
int inputBufferIdx = 0;
int lastInputBufferIdx = 0;

void LoadImage()
{
	const char *image_file_name = "Images/Image";

	FILE *f = fopen(image_file_name, "rb");
	if (!f || ferror(f))
	{
		fprintf(stderr, "Error: \"%s\" not found\n", image_file_name);
		exit(-5);
	}
	fseek(f, 0, SEEK_END);
	long flen = ftell(f);
	fseek(f, 0, SEEK_SET);
	if (flen > ram_amt)
	{
		fprintf(stderr, "Error: Could not fit RAM image (%ld bytes) into %d\n", flen, ram_amt);
		exit(-7);
	}

	memset(ram_image, 0, ram_amt);
	if (fread(ram_image, flen, 1, f) != 1)
	{
		fprintf(stderr, "Error: Could not load image.\n");
		exit(-7);
	}
	fclose(f);

	int dtb_ptr = ram_amt - sizeof(default64mbdtb) - sizeof(struct MiniRV32IMAState);
	memcpy(ram_image + dtb_ptr, default64mbdtb, sizeof(default64mbdtb));
	if (kernel_command_line)
	{
		strncpy((char *)(ram_image + dtb_ptr + 0xc0), kernel_command_line, 54);
	}

	// The core lives at the end of RAM.
	core = (struct MiniRV32IMAState *)(ram_image + ram_amt - sizeof(struct MiniRV32IMAState));
	core->pc = MINIRV32_RAM_IMAGE_OFFSET;
	core->regs[10] = 0x00;												  // hart ID
	core->regs[11] = dtb_ptr ? (dtb_ptr + MINIRV32_RAM_IMAGE_OFFSET) : 0; // dtb_pa (Must be valid pointer) (Should be pointer to dtb)
	core->extraflags |= 3;												  // Machine-mode.

	// Update system ram size in DTB (but if and only if we're using the default DTB)
	// Warning - this will need to be updated if the skeleton DTB is ever modified.
	uint32_t *dtb = (uint32_t *)(ram_image + dtb_ptr);
	if (dtb[0x13c / 4] == 0x00c0ff03)
	{
		uint32_t validram = dtb_ptr;
		dtb[0x13c / 4] = (validram >> 24) | (((validram >> 16) & 0xff) << 8) | (((validram >> 8) & 0xff) << 16) | ((validram & 0xff) << 24);
	}
}

int do_sleep = 1;
int time_divisor = 1;
int fixed_update = 0;

uint64_t rt;

char* GetOutputBuffer() {
	return outputBuffer;
}

int GetOutputBufferLength() {
	return outputBufferCount;
}

void ClearOutputBuffer() {
	outputBufferCount = 0;
}

void SetInputBufferSymbol(char ch) {
	inputBuffer[inputBufferIdx++] = ch;
}

void PerformOneCycle()
{
	uint64_t lastTime = (fixed_update) ? 0 : (GetTimeMicroseconds() / time_divisor);

	for (int i = 0; i < 1024; i++) {
		uint64_t *this_ccount = ((uint64_t *)&core->cyclel);
		uint32_t elapsedUs = GetTimeMicroseconds() / time_divisor - lastTime;
		lastTime += elapsedUs;
		MiniRV32IMAStep(core, ram_image, 0, elapsedUs, 1024);
	}
}

void Initialize()
{
	ram_image = malloc(ram_amt);
	if (!ram_image)
	{
		fprintf(stderr, "Error: could not allocate system image.\n");
		exit(-4);
	}
	LoadImage();
}

int main(int argc, char **argv)
{
	Initialize();
	for (;;) {
		PerformOneCycle();
	}
}

#include <unistd.h>
#include <sys/time.h>

static void MiniSleep()
{
	usleep(500);
}

static uint64_t GetTimeMicroseconds()
{
	struct timeval tv;
	gettimeofday(&tv, 0);
	return tv.tv_usec + ((uint64_t)(tv.tv_sec)) * 1000000LL;
}

static int is_eofd;

static int ReadKBByte()
{
	if (is_eofd)
		return 0xffffffff;

	return inputBuffer[lastInputBufferIdx++];
}

static int IsKBHit()
{
	return inputBufferIdx > lastInputBufferIdx;
}

//////////////////////////////////////////////////////////////////////////
// Rest of functions functionality
//////////////////////////////////////////////////////////////////////////

static uint32_t HandleException(uint32_t ir, uint32_t code)
{
	// Weird opcode emitted by duktape on exit.
	if (code == 3)
	{
		// Could handle other opcodes here.
	}
	return code;
}

static uint32_t HandleControlStore(uint32_t addy, uint32_t val)
{
	if (addy == 0x10000000) // UART 8250 / 16550 Data Buffer
	{
		outputBuffer[outputBufferCount++] = val;
	}
	else if (addy == 0x11004004) // CLNT
		core->timermatchh = val;
	else if (addy == 0x11004000) // CLNT
		core->timermatchl = val;
	else if (addy == 0x11100000) // SYSCON (reboot, poweroff, etc.)
	{
		core->pc = core->pc + 4;
		return val; // NOTE: PC will be PC of Syscon.
	}
	return 0;
}

static uint32_t HandleControlLoad(uint32_t addy)
{
	// Emulating a 8250 / 16550 UART
	if (addy == 0x10000005)
		return 0x60 | IsKBHit();
	else if (addy == 0x10000000 && IsKBHit()) {
		return ReadKBByte();
	}
	else if (addy == 0x1100bffc) // https://chromitem-soc.readthedocs.io/en/latest/clint.html
		return core->timerh;
	else if (addy == 0x1100bff8)
		return core->timerl;
	return 0;
}
#include <GLFW/glfw3.h>
#include "glouboy.h"
#include "io.h"
#include "video.h"
#include "timer.h"

#define interruptEnable ram[IO_REGISTER | IE]
#define interruptFlags ram[IO_REGISTER | IF]

bool InterruptMasterFlag = true;

void disableInterrupt()
{
	InterruptMasterFlag = 0;
}

int shouldRiseInterruptMasterFlag = 0;

void delayedEnableInterrupt()
{
	shouldRiseInterruptMasterFlag = 1;
}

void enableInterrupt()
{
	InterruptMasterFlag = 1;
}

void trigInterrupt(int interrupt)
{
	interruptFlags |= interrupt;
}

bool handleInterrupt(int flag, int vector)
{
	if (flag & interruptEnable & interruptFlags)
	{
		if (InterruptMasterFlag)
		{
			interruptFlags ^= flag;
			jumpToVector(vector);
			disableInterrupt();
			return true;
		}
	}
	return false;
}

void handleInterrupts()
{
	if (interruptEnable & interruptFlags)
	{
		wakeHalteMode();
	}

	if (!handleInterrupt(IRQ_V_Blank, 0x40))
	if (!handleInterrupt(IRQ_LCDC, 0x48))
	if (!handleInterrupt(IRQ_TIMER, 0x50))
	if (!handleInterrupt(IRQ_SERIAL, 0x58))
	if (!handleInterrupt(IRQ_JOYPAD, 0x60)) {
	}

	// InterruptMasterFlag is not set immediatelly
	if (shouldRiseInterruptMasterFlag == 1)
	{
		shouldRiseInterruptMasterFlag = 0;
		InterruptMasterFlag = true;
	}

}

void writeRam(unsigned short addr, int value)
{
	if ((addr & 0xff00) == 0xff00)
	{
		value = writeIO(0x00ff & addr, value);
	}

	if (addr < 0x8000)
	{
		return;
	}

	ram[addr] = value;
}

unsigned char button;

unsigned char direction;

int writeIO(unsigned short registerAddr, int value)
{
	if ((registerAddr & 0x40) == 0x40) // video ctrl
	{
		value = videoWrite(registerAddr, value);
	}
	if (registerAddr == DIV) // timer DIV
	{
		value = timerDivWrite();
	}
	if (registerAddr == DMA) // timer DIV
	{
		int origin = (value << 8) & 0xFF00;
		memcpy(ram.data() + OAM, ram.data() + origin, 160);
	}

	if (registerAddr == P1) // timer DIV
	{
		if((value & (1 << 4)) == 0)
			value = 0xC0 | (1 << 4) | direction;
		else if ((value & (1 << 5)) == 0)
			value = 0xC0 | (1 << 5) | button;
		else
			value = 0xff;
	}

	return value;
}

void handleJoypad()
{
	int axes_count = 0, buttons_count = 0;
	const float* axes = glfwGetJoystickAxes(GLFW_JOYSTICK_1, &axes_count);
	const unsigned char* buttons = glfwGetJoystickButtons(GLFW_JOYSTICK_1, &buttons_count);
	if (buttons_count > 13)
	{
		button = (buttons[7] << 3 | buttons[6] << 2 | buttons[0] <<1 | buttons[1]) ^ 0x0f; // start - select - B - A
		direction = buttons[12] << 3 | buttons[10] << 2 | buttons[13] << 1 | buttons[11] ^ 0x0f; // down up left right
	}
}
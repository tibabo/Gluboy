#include <GLFW/glfw3.h>
#include <string.h>
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


	// echo memory
	if (addr == 0xC202)
	{
		int i = 0;
		i++;
	}

	// echo memory
	if ((addr >= 0xC000) && (addr <= 0xDDFF))
	{
		ram[addr - 0xC000 + 0xE000] = value;
	}
	if ((addr >= 0xE000) && (addr <= 0xFDFF))
	{
		ram[addr - 0xE000 + 0xC000] = value;
	}

	if (addr < 0x8000)
	{
		int cartType = ram[0x0147];
		if ((cartType > 0 ) && (cartType < 4))
		{
			static char upper = 0;
			static char bank = 0;
			if ((addr >= 0x4000) && (addr <= 0x5fff))
			{
				upper = value;
			}
			if ((addr >= 0x2000) && (addr <= 0x3fff))
			{
				bank = value ? value : 1;
			}
			memcpy(ram + 0x4000, rom + 0x4000 * ((upper << 5) | bank), 0x4000);
		}
		return;
	}

	ram[addr] = value;
}

unsigned char button    = 0x0f, newButton    = 0x0f;
unsigned char direction = 0x0f, newDirection = 0x0f;

int writeIO(unsigned short registerAddr, int value)
{
	if ((registerAddr & 0x40) == 0x40) // video ctrl
	{
		value = videoWrite(registerAddr, value);
	}
	if (registerAddr == DIV)
	{
		value = timerDivWrite();
	}

	if (registerAddr == DMA)
	{
		unsigned short origin = (value << 8) & 0xFF00;
		memcpy(ram + OAM, ram + origin, 160);
	}

	if (registerAddr == P1) 
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
	static int target = 456 * 154;
	target -= TC;
	if (target > 0)
	{
		return;
	}
	target = 456 * 154;

	int axes_count = 0, buttons_count = 0;
	const float* axes = glfwGetJoystickAxes(GLFW_JOYSTICK_1, &axes_count);
	const unsigned char* buttons = glfwGetJoystickButtons(GLFW_JOYSTICK_1, &buttons_count);
	if (buttons_count > 13)
	{
		newButton = (buttons[7] << 3 | buttons[6] << 2 | buttons[0] <<1 | buttons[1]) ^ 0x0f; // start - select - B - A
		newDirection = (buttons[12] << 3 | buttons[10] << 2 | buttons[13] << 1 | buttons[11]) ^ 0x0f; // down up left right
	//	if ((ram[IO_REGISTER | P1] & (1 << 4)) != (1 << 4))
		{
			if (newDirection < direction) 
			{
				trigInterrupt(IRQ_JOYPAD);
			}
		}
		direction = newDirection;
	//	if ((ram[IO_REGISTER | P1] & (1 << 5)) != (1 << 5))
		{
			if (newButton < button)
			{
				trigInterrupt(IRQ_JOYPAD);
			}
		}
		button = newButton;
	}
}
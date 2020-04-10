#include <GLFW/glfw3.h>
#include "imgui.h"
#include <string.h>
#include "glouboy.h"
#include "io.h"
#include "video.h"
#include "audio.h"
#include "timer.h"
#include "cpu.h"


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
		//MBC1
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
		//MBC3
		if ((cartType >= 0x0F) && (cartType <= 0x13))
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
		value = video->write(registerAddr, value);
	}

	if (registerAddr == NR10)
	{
		audio->sweep.updateValue(value);
	}
	if (registerAddr == NR14)
	{
		audio->updateChannel(0, value);
	}
	if (registerAddr == NR24)
	{
		audio->updateChannel(1, value);
	}
	if (registerAddr == NR34)
	{
		audio->updateChannel(2, value);
	}

	if (registerAddr == NR43)
	{
		audio->noise.reloadTimer(value);
	}

	if (registerAddr == NR44)
	{
		audio->updateChannel(3, value);
	}

	if (registerAddr == DIV)
	{
		value = timer->divWrite();
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

bool handleJoypad()
{
	int axes_count = 0;
	int buttons_count = 0;
	const float* axes = glfwGetJoystickAxes(GLFW_JOYSTICK_1, &axes_count);
	const unsigned char* buttons = glfwGetJoystickButtons(GLFW_JOYSTICK_1, &buttons_count);
	if (buttons_count > 13)
	{
		if (buttons[4]) return true; // for rewind
		bool start = ImGui::IsKeyDown(257) | buttons[7];
		bool select = ImGui::IsKeyDown(258) | buttons[6];
		bool b = ImGui::IsKeyDown(81) | buttons[0];
		bool a = ImGui::IsKeyDown(87) | buttons[1];
		newButton = (start << 3 | select << 2 | b << 1 | a) ^ 0x0f; // start - select - B - A

		float treshold = 0.3f;
		bool left = ImGui::IsKeyDown(263) | buttons[13] | axes[0] < -treshold;
		bool right = ImGui::IsKeyDown(262) | buttons[11] | axes[0] > treshold;
		bool up = ImGui::IsKeyDown(265) | buttons[10] | axes[1] < -treshold;
		bool down = ImGui::IsKeyDown(264) | buttons[12] | axes[1] > treshold;
		newDirection = (down << 3 | up << 2 | left << 1 | right) ^ 0x0f; // down up left right

		

		{
			if (newDirection < direction) 
			{
				cpu->trigInterrupt(IRQ_JOYPAD);
			}
		}
		direction = newDirection;

		{
			if (newButton < button)
			{
				cpu->trigInterrupt(IRQ_JOYPAD);
			}
		}
		button = newButton;
	}
	return false;
}
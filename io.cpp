#include "glouboy.h"
#include "io.h"
#include "video.h"


#define interruptEnable ram[IO_REGISTER | IE]
#define interruptFlags ram[IO_REGISTER | IF]

bool InterruptMasterFlag = 0;

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

void writeRam(int addr, int value)
{
	if ((addr & 0xff00) == 0xff00)
	{
		writeIO(0x00ff & addr, value);
	}

	if (addr < 0x8000)
	{
		return;
	}

	ram[addr] = value;
}

void writeIO(int registerAddr, int value)
{
	if ((registerAddr & 0x40) == 0x40) // video ctrl
	{
		videoWrite(registerAddr, value);
	}
	if ((registerAddr == 2) || (registerAddr == 5) || (registerAddr == 6) || (registerAddr == 7)) // timer
	{
		int i = 0;
		i++;
	}
}